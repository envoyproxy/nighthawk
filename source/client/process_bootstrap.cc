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

using envoy::config::bootstrap::v3::Bootstrap;

// Adds a cluster for the request source into the bootstrap.
void addRequestSourceCluster(const Client::Options& options, const Uri& uri, int worker_number,
                             Bootstrap& bootstrap) {
  auto* cluster = bootstrap.mutable_static_resources()->add_clusters();
  cluster->mutable_http2_protocol_options();
  cluster->set_name(fmt::format("{}.requestsource", worker_number));
  cluster->set_type(
      envoy::config::cluster::v3::Cluster::DiscoveryType::Cluster_DiscoveryType_STATIC);
  cluster->mutable_connect_timeout()->set_seconds(options.timeout().count());

  auto* load_assignment = cluster->mutable_load_assignment();
  load_assignment->set_cluster_name(cluster->name());
  auto* socket = cluster->mutable_load_assignment()
                     ->add_endpoints()
                     ->add_lb_endpoints()
                     ->mutable_endpoint()
                     ->mutable_address()
                     ->mutable_socket_address();
  socket->set_address(uri.address()->ip()->addressAsString());
  socket->set_port_value(uri.port());
}

} // namespace

absl::StatusOr<Bootstrap> createBootstrapConfiguration(const Client::Options& options,
                                                       const std::vector<UriPtr>& uris,
                                                       const UriPtr& request_source_uri,
                                                       int number_of_workers) {
  Bootstrap bootstrap;
  for (int i = 0; i < number_of_workers; i++) {
    auto* cluster = bootstrap.mutable_static_resources()->add_clusters();

    if (uris.empty()) {
      return absl::InvalidArgumentError(
          "illegal configuration with zero endpoints, at least one uri must be specified");
    }
    if (uris[0]->scheme() == "https") {
      auto* transport_socket = cluster->mutable_transport_socket();
      transport_socket->set_name("envoy.transport_sockets.tls");
      envoy::extensions::transport_sockets::tls::v3::UpstreamTlsContext context =
          options.tlsContext();
      const std::string sni_host = Client::SniUtility::computeSniHost(
          uris, options.requestHeaders(), options.upstreamProtocol());
      if (!sni_host.empty()) {
        *context.mutable_sni() = sni_host;
      }
      auto* common_tls_context = context.mutable_common_tls_context();
      if (options.upstreamProtocol() == Envoy::Http::Protocol::Http2) {
        common_tls_context->add_alpn_protocols("h2");
      } else if (options.upstreamProtocol() == Envoy::Http::Protocol::Http3) {
        return absl::UnimplementedError("HTTP/3 Quic support isn't implemented yet.");
      } else {
        common_tls_context->add_alpn_protocols("http/1.1");
      }
      transport_socket->mutable_typed_config()->PackFrom(context);
    }
    if (options.transportSocket().has_value()) {
      *cluster->mutable_transport_socket() = options.transportSocket().value();
    }
    cluster->set_name(fmt::format("{}", i));
    cluster->mutable_connect_timeout()->set_seconds(options.timeout().count());
    cluster->mutable_max_requests_per_connection()->set_value(options.maxRequestsPerConnection());
    if (options.upstreamProtocol() == Envoy::Http::Protocol::Http2) {
      auto* cluster_http2_protocol_options = cluster->mutable_http2_protocol_options();
      cluster_http2_protocol_options->mutable_max_concurrent_streams()->set_value(
          options.maxConcurrentStreams());
    }

    auto thresholds = cluster->mutable_circuit_breakers()->add_thresholds();
    // We do not support any retrying.
    thresholds->mutable_max_retries()->set_value(0);
    thresholds->mutable_max_connections()->set_value(options.connections());
    // We specialize on 0 below, as that is not supported natively. The benchmark client will track
    // in flight work and avoid creating pending requests in this case.
    thresholds->mutable_max_pending_requests()->set_value(
        options.maxPendingRequests() == 0 ? 1 : options.maxPendingRequests());
    thresholds->mutable_max_requests()->set_value(options.maxActiveRequests());

    cluster->set_type(
        envoy::config::cluster::v3::Cluster::DiscoveryType::Cluster_DiscoveryType_STATIC);

    auto* load_assignment = cluster->mutable_load_assignment();
    load_assignment->set_cluster_name(cluster->name());
    auto* endpoints = cluster->mutable_load_assignment()->add_endpoints();
    for (const UriPtr& uri : uris) {
      auto* socket = endpoints->add_lb_endpoints()
                         ->mutable_endpoint()
                         ->mutable_address()
                         ->mutable_socket_address();
      socket->set_address(uri->address()->ip()->addressAsString());
      socket->set_port_value(uri->port());
    }
    if (request_source_uri != nullptr) {
      addRequestSourceCluster(options, *request_source_uri, i, bootstrap);
    }
  }

  for (const envoy::config::metrics::v3::StatsSink& stats_sink : options.statsSinks()) {
    *bootstrap.add_stats_sinks() = stats_sink;
  }
  bootstrap.mutable_stats_flush_interval()->set_seconds(options.statsFlushInterval());
  return bootstrap;
}

} // namespace Nighthawk
