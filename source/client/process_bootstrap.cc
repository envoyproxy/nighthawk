#include "source/client/process_bootstrap.h"

#include <string>
#include <vector>

#include "nighthawk/client/options.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy_api/envoy/config/bootstrap/v3/bootstrap.pb.h"

#include "source/client/sni_utility.h"

namespace Nighthawk {
namespace {

using ::envoy::config::bootstrap::v3::Bootstrap;
using ::envoy::config::cluster::v3::CircuitBreakers;
using ::envoy::config::cluster::v3::Cluster;
using ::envoy::config::core::v3::SocketAddress;
using ::envoy::config::core::v3::TransportSocket;
using ::envoy::config::endpoint::v3::ClusterLoadAssignment;
using ::envoy::config::endpoint::v3::LocalityLbEndpoints;
using ::envoy::config::metrics::v3::StatsSink;
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
  cluster.mutable_http2_protocol_options();
  cluster.set_name(fmt::format("{}.requestsource", worker_number));
  cluster.set_type(Cluster::DiscoveryType::Cluster_DiscoveryType_STATIC);
  cluster.mutable_connect_timeout()->set_seconds(options.timeout().count());

  ClusterLoadAssignment* load_assignment = cluster.mutable_load_assignment();
  load_assignment->set_cluster_name(cluster.name());
  LocalityLbEndpoints* endpoints = load_assignment->add_endpoints();
  addUriToEndpoints(request_source_uri, endpoints);
  return cluster;
}

// Determines whether the generated bootstrap requires transport socket
// configuration.
// Transport socket is required of the URI scheme is "https", or if the user
// specified a custom transport socket on the command line.
bool needTransportSocket(const Client::Options& options, const std::vector<UriPtr>& uris) {
  return uris[0]->scheme() == "https" || options.transportSocket().has_value();
}

// Creates the transport socket configuration.
absl::StatusOr<TransportSocket> createTransportSocket(const Client::Options& options,
                                                      const std::vector<UriPtr>& uris) {
  // User specified a transport socket configuration takes precedence.
  if (options.transportSocket().has_value()) {
    return options.transportSocket().value();
  }

  TransportSocket transport_socket;
  transport_socket.set_name("envoy.transport_sockets.tls");

  UpstreamTlsContext upstream_tls_context = options.tlsContext();
  const std::string sni_host = Client::SniUtility::computeSniHost(uris, options.requestHeaders(),
                                                                  options.upstreamProtocol());
  if (!sni_host.empty()) {
    *upstream_tls_context.mutable_sni() = sni_host;
  }

  CommonTlsContext* common_tls_context = upstream_tls_context.mutable_common_tls_context();
  if (options.upstreamProtocol() == Envoy::Http::Protocol::Http2) {
    common_tls_context->add_alpn_protocols("h2");

  } else if (options.upstreamProtocol() == Envoy::Http::Protocol::Http3) {
    return absl::UnimplementedError("HTTP/3 Quic support isn't implemented yet.");

  } else {
    common_tls_context->add_alpn_protocols("http/1.1");
  }

  transport_socket.mutable_typed_config()->PackFrom(upstream_tls_context);
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
  cluster.mutable_max_requests_per_connection()->set_value(options.maxRequestsPerConnection());

  if (options.upstreamProtocol() == Envoy::Http::Protocol::Http2) {
    auto* cluster_http2_protocol_options = cluster.mutable_http2_protocol_options();
    cluster_http2_protocol_options->mutable_max_concurrent_streams()->set_value(
        options.maxConcurrentStreams());
  }

  *cluster.mutable_circuit_breakers() = createCircuitBreakers(options);

  cluster.set_type(Cluster::DiscoveryType::Cluster_DiscoveryType_STATIC);

  ClusterLoadAssignment* load_assignment = cluster.mutable_load_assignment();
  load_assignment->set_cluster_name(cluster.name());
  LocalityLbEndpoints* endpoints = load_assignment->add_endpoints();
  for (const UriPtr& uri : uris) {
    addUriToEndpoints(*uri, endpoints);
  }
  return cluster;
}

} // namespace

absl::StatusOr<Bootstrap> createBootstrapConfiguration(const Client::Options& options,
                                                       const std::vector<UriPtr>& uris,
                                                       const UriPtr& request_source_uri,
                                                       int number_of_workers) {
  Bootstrap bootstrap;

  for (int worker_number = 0; worker_number < number_of_workers; worker_number++) {
    if (uris.empty()) {
      return absl::InvalidArgumentError(
          "illegal configuration with zero endpoints, at least one uri must be specified");
    }

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
  bootstrap.mutable_stats_flush_interval()->set_seconds(options.statsFlushInterval());

  return bootstrap;
}

} // namespace Nighthawk
