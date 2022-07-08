#include "source/client/process_bootstrap.h"

#include <string>
#include <vector>

#include "nighthawk/client/options.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy_api/envoy/config/bootstrap/v3/bootstrap.pb.h"
#include "external/envoy_api/envoy/extensions/transport_sockets/quic/v3/quic_transport.pb.h"
#include "external/envoy_api/envoy/extensions/upstreams/http/v3/http_protocol_options.pb.h"

#include "source/client/sni_utility.h"
#include "source/common/uri_impl.h"
#include "source/common/utility.h"

namespace Nighthawk {
namespace {

using ::envoy::config::bootstrap::v3::Bootstrap;
using ::envoy::config::cluster::v3::CircuitBreakers;
using ::envoy::config::cluster::v3::Cluster;
using ::envoy::config::core::v3::Http2ProtocolOptions;
using ::envoy::config::core::v3::Http3ProtocolOptions;
using ::envoy::config::core::v3::SocketAddress;
using ::envoy::config::core::v3::TransportSocket;
using ::envoy::config::endpoint::v3::ClusterLoadAssignment;
using ::envoy::config::endpoint::v3::LocalityLbEndpoints;
using ::envoy::config::metrics::v3::StatsSink;
using ::envoy::extensions::transport_sockets::quic::v3::QuicUpstreamTransport;
using ::envoy::extensions::transport_sockets::tls::v3::CommonTlsContext;
using ::envoy::extensions::transport_sockets::tls::v3::UpstreamTlsContext;

// Adds the address and port specified in the URI to the endpoints.
void addUriToEndpoints(const Uri& uri, LocalityLbEndpoints* endpoints) {
  SocketAddress* socket_address = endpoints->add_lb_endpoints()
                                      ->mutable_endpoint()
                                      ->mutable_address()
                                      ->mutable_socket_address();
  socket_address->set_address(uri.address()->ip()->addressAsString());
  socket_address->set_port_value(uri.port());
}

// Creates a cluster used for communication with request source for the
// specified worker number.
Cluster createRequestSourceClusterForWorker(const Client::Options& options,
                                            const Uri& request_source_uri, int worker_number) {
  Cluster cluster;

  envoy::extensions::upstreams::http::v3::HttpProtocolOptions http_options;
  http_options.mutable_explicit_http_config()->mutable_http2_protocol_options();
  (*cluster.mutable_typed_extension_protocol_options())
      ["envoy.extensions.upstreams.http.v3.HttpProtocolOptions"]
          .PackFrom(http_options);

  cluster.set_name(fmt::format("{}.requestsource", worker_number));
  cluster.set_type(Cluster::STATIC);
  cluster.mutable_connect_timeout()->set_seconds(options.timeout().count());

  ClusterLoadAssignment* load_assignment = cluster.mutable_load_assignment();
  load_assignment->set_cluster_name(cluster.name());
  LocalityLbEndpoints* endpoints = load_assignment->add_endpoints();
  addUriToEndpoints(request_source_uri, endpoints);
  return cluster;
}

// Determines whether the generated bootstrap requires transport socket
// configuration.
// Transport socket is required if the URI scheme is "https", or if the user
// specified a custom transport socket on the command line.
bool needTransportSocket(const Client::Options& options, const std::vector<UriPtr>& uris) {
  return uris[0]->scheme() == "https" || options.transportSocket().has_value();
}

// Creates the transport socket configuration.
absl::StatusOr<TransportSocket> createTransportSocket(const Client::Options& options,
                                                      const std::vector<UriPtr>& uris) {
  // User specified transport socket configuration takes precedence.
  if (options.transportSocket().has_value()) {
    return options.transportSocket().value();
  }

  TransportSocket transport_socket;

  UpstreamTlsContext upstream_tls_context = options.tlsContext();
  const std::string sni_host =
      Client::SniUtility::computeSniHost(uris, options.requestHeaders(), options.protocol());
  if (!sni_host.empty()) {
    *upstream_tls_context.mutable_sni() = sni_host;
  }

  CommonTlsContext* common_tls_context = upstream_tls_context.mutable_common_tls_context();
  if (options.protocol() == Envoy::Http::Protocol::Http2) {
    transport_socket.set_name("envoy.transport_sockets.tls");
    common_tls_context->add_alpn_protocols("h2");
    transport_socket.mutable_typed_config()->PackFrom(upstream_tls_context);

  } else if (options.protocol() == Envoy::Http::Protocol::Http3) {
    transport_socket.set_name("envoy.transport_sockets.quic");
    common_tls_context->add_alpn_protocols("h3");

    QuicUpstreamTransport quic_upstream_transport;
    *quic_upstream_transport.mutable_upstream_tls_context() = upstream_tls_context;
    transport_socket.mutable_typed_config()->PackFrom(quic_upstream_transport);

  } else {
    transport_socket.set_name("envoy.transport_sockets.tls");
    common_tls_context->add_alpn_protocols("http/1.1");
    transport_socket.mutable_typed_config()->PackFrom(upstream_tls_context);
  }

  return transport_socket;
}

// Creates circuit breakers configuration based on the specified options.
CircuitBreakers createCircuitBreakers(const Client::Options& options) {
  CircuitBreakers circuit_breakers;
  CircuitBreakers::Thresholds* thresholds = circuit_breakers.add_thresholds();

  // We do not support any retrying.
  thresholds->mutable_max_retries()->set_value(0);
  thresholds->mutable_max_connections()->set_value(options.connections());

  // We specialize on 0 below, as that is not supported natively. The benchmark client will track
  // in flight work and avoid creating pending requests in this case.
  thresholds->mutable_max_pending_requests()->set_value(
      options.maxPendingRequests() == 0 ? 1 : options.maxPendingRequests());
  thresholds->mutable_max_requests()->set_value(options.maxActiveRequests());

  return circuit_breakers;
}

// Creates a cluster used by Nighthawk to upstream requests to the uris by the specified worker
// number.
Cluster createNighthawkClusterForWorker(const Client::Options& options,
                                        const std::vector<UriPtr>& uris, int worker_number) {
  Cluster cluster;

  cluster.set_name(fmt::format("{}", worker_number));
  cluster.mutable_connect_timeout()->set_seconds(options.timeout().count());

  envoy::extensions::upstreams::http::v3::HttpProtocolOptions http_options;
  http_options.mutable_common_http_protocol_options()
      ->mutable_max_requests_per_connection()
      ->set_value(options.maxRequestsPerConnection());

  if (options.protocol() == Envoy::Http::Protocol::Http2) {
    Http2ProtocolOptions* http2_options =
        http_options.mutable_explicit_http_config()->mutable_http2_protocol_options();
    http2_options->mutable_max_concurrent_streams()->set_value(options.maxConcurrentStreams());

  } else if (options.protocol() == Envoy::Http::Protocol::Http3) {
    if (options.http3ProtocolOptions().has_value()) {
      *http_options.mutable_explicit_http_config()->mutable_http3_protocol_options() =
          options.http3ProtocolOptions().value();
    } else {
      Http3ProtocolOptions* http3_options =
          http_options.mutable_explicit_http_config()->mutable_http3_protocol_options();
      http3_options->mutable_quic_protocol_options()->mutable_max_concurrent_streams()->set_value(
          options.maxConcurrentStreams());
    }
  } else {
    http_options.mutable_explicit_http_config()->mutable_http_protocol_options();
  }

  (*cluster.mutable_typed_extension_protocol_options())
      ["envoy.extensions.upstreams.http.v3.HttpProtocolOptions"]
          .PackFrom(http_options);

  *cluster.mutable_circuit_breakers() = createCircuitBreakers(options);

  cluster.set_type(Cluster::STATIC);

  ClusterLoadAssignment* load_assignment = cluster.mutable_load_assignment();
  load_assignment->set_cluster_name(cluster.name());
  LocalityLbEndpoints* endpoints = load_assignment->add_endpoints();
  for (const UriPtr& uri : uris) {
    addUriToEndpoints(*uri, endpoints);
  }
  return cluster;
}

// Extracts URIs of the targets and the request source (if specified) from the
// Nighthawk options.
// Resolves all the extracted URIs.
absl::Status extractAndResolveUrisFromOptions(Envoy::Event::Dispatcher& dispatcher,
                                              const Client::Options& options,
                                              Envoy::Network::DnsResolver& dns_resolver,
                                              std::vector<UriPtr>* uris,
                                              UriPtr* request_source_uri) {
  try {
    if (options.uri().has_value()) {
      uris->push_back(std::make_unique<UriImpl>(options.uri().value()));
    } else {
      for (const nighthawk::client::MultiTarget::Endpoint& endpoint :
           options.multiTargetEndpoints()) {
        uris->push_back(std::make_unique<UriImpl>(fmt::format(
            "{}://{}:{}{}", options.multiTargetUseHttps() ? "https" : "http",
            endpoint.address().value(), endpoint.port().value(), options.multiTargetPath())));
      }
    }
    for (const UriPtr& uri : *uris) {
      uri->resolve(dispatcher, dns_resolver,
                   Utility::translateFamilyOptionString(options.addressFamily()));
    }
    if (options.requestSource() != "") {
      *request_source_uri = std::make_unique<UriImpl>(options.requestSource());
      (*request_source_uri)
          ->resolve(dispatcher, dns_resolver,
                    Utility::translateFamilyOptionString(options.addressFamily()));
    }
  } catch (const UriException& ex) {
    return absl::InvalidArgumentError(
        fmt::format("URI exception (for example, malformed URI syntax, bad MultiTarget path, "
                    "unresolvable host DNS): {}",
                    ex.what()));
  }
  return absl::OkStatus();
}

} // namespace

absl::StatusOr<Bootstrap> createBootstrapConfiguration(
    Envoy::Event::Dispatcher& dispatcher, Envoy::Api::Api& api, const Client::Options& options,
    Envoy::Network::DnsResolverFactory& dns_resolver_factory,
    const envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config,
    int number_of_workers) {
  Envoy::Network::DnsResolverSharedPtr dns_resolver =
      dns_resolver_factory.createDnsResolver(dispatcher, api, typed_dns_resolver_config);
  std::vector<UriPtr> uris;
  UriPtr request_source_uri;
  absl::Status uri_status = extractAndResolveUrisFromOptions(dispatcher, options, *dns_resolver,
                                                             &uris, &request_source_uri);
  if (!uri_status.ok()) {
    return uri_status;
  }

  Bootstrap bootstrap;
  for (int worker_number = 0; worker_number < number_of_workers; worker_number++) {
    Cluster nighthawk_cluster = createNighthawkClusterForWorker(options, uris, worker_number);

    if (needTransportSocket(options, uris)) {
      absl::StatusOr<TransportSocket> transport_socket = createTransportSocket(options, uris);
      if (!transport_socket.ok()) {
        return transport_socket.status();
      }
      *nighthawk_cluster.mutable_transport_socket() = *transport_socket;
    }
    *bootstrap.mutable_static_resources()->add_clusters() = nighthawk_cluster;

    if (request_source_uri != nullptr) {
      *bootstrap.mutable_static_resources()->add_clusters() =
          createRequestSourceClusterForWorker(options, *request_source_uri, worker_number);
    }
  }

  for (const StatsSink& stats_sink : options.statsSinks()) {
    *bootstrap.add_stats_sinks() = stats_sink;
  }

  if (options.statsFlushIntervalDuration().seconds() > 0 ||
      options.statsFlushIntervalDuration().nanos() > 0) {
    *bootstrap.mutable_stats_flush_interval() = options.statsFlushIntervalDuration();
  } else {
    bootstrap.mutable_stats_flush_interval()->set_seconds(options.statsFlushInterval());
  }

  if (options.upstreamBindConfig().has_value()) {
    *bootstrap.mutable_cluster_manager()->mutable_upstream_bind_config() =
        options.upstreamBindConfig().value();
  }

  return bootstrap;
}

} // namespace Nighthawk
