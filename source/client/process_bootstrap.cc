#include "source/client/process_bootstrap.h"

#include <string>
#include <vector>

#include "nighthawk/client/options.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy_api/envoy/config/bootstrap/v3/bootstrap.pb.h"
#include "external/envoy_api/envoy/extensions/transport_sockets/quic/v3/quic_transport.pb.h"
#include "external/envoy_api/envoy/extensions/upstreams/http/v3/http_protocol_options.pb.h"

#include "external/envoy_api/envoy/extensions/filters/udp/udp_proxy/v3/udp_proxy.pb.h"
#include "external/envoy_api/envoy/extensions/filters/network/tcp_proxy/v3/tcp_proxy.pb.h"
#include "external/envoy_api/envoy/extensions/filters/udp/udp_proxy/v3/route.pb.h"
#include "external/envoy_api/envoy/extensions/filters/udp/udp_proxy/session/http_capsule/v3/http_capsule.pb.h"

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
    http2_options->mutable_use_oghttp2_codec()->set_value(false);

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
                                              UriPtr &encap_uri,
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
    if(!options.tunnelUri().empty()){
      encap_uri = std::make_unique<UriImpl>(fmt::format("https://localhost:{}", options.encapPort()));
      encap_uri->resolve(dispatcher, dns_resolver,
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
  absl::StatusOr<Envoy::Network::DnsResolverSharedPtr> dns_resolver =
      dns_resolver_factory.createDnsResolver(dispatcher, api, typed_dns_resolver_config);
  if (!dns_resolver.ok()) {
    return dns_resolver.status();
  }
  // resolve targets and encapsulation 
  std::vector<UriPtr> uris;
  UriPtr request_source_uri, encap_uri;
  absl::Status uri_status = extractAndResolveUrisFromOptions(
      dispatcher, options, *dns_resolver.value(), encap_uri ,&uris, &request_source_uri);
  if (!uri_status.ok()) {
    return uri_status;
  }

  Bootstrap bootstrap;
  for (int worker_number = 0; worker_number < number_of_workers; worker_number++) {
    bool is_tunneling = !options.tunnelUri().empty();
    // if we're tunneling, redirect traffic to the encap listener
    // while maintaining the host value
    std::vector<UriPtr> encap_uris;
    encap_uris.push_back(std::move(encap_uri));
    if(is_tunneling && encap_uris.empty()){
      return absl::InvalidArgumentError("No encapsulation URI for tunneling");
    }
    Cluster nighthawk_cluster = is_tunneling ? createNighthawkClusterForWorker(options, encap_uris, worker_number)
                               : createNighthawkClusterForWorker(options, uris, worker_number); 

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

absl::StatusOr<envoy::config::bootstrap::v3::Bootstrap> createEncapBootstrap(const Client::Options& options, UriImpl& tunnel_uri, Envoy::Event::Dispatcher& dispatcher, const Envoy::Network::DnsResolverSharedPtr& dns_resolver)
{
  envoy::config::bootstrap::v3::Bootstrap encap_bootstrap;

  // CONNECT-UDP for HTTP3.
  bool is_udp = options.protocol() == Envoy::Http::Protocol::Http3;
  auto tunnel_protocol = options.tunnelProtocol();

  // Create encap bootstrap.
  auto *listener = encap_bootstrap.mutable_static_resources()->add_listeners();
  listener->set_name("encap_listener");
  auto *address = listener->mutable_address();
  auto *socket_address = address->mutable_socket_address();

  UriImpl encap_uri(fmt::format("http://localhost:{}", options.encapPort()));
  encap_uri.resolve(dispatcher, *dns_resolver,
                Utility::translateFamilyOptionString(options.addressFamily()));
  
  socket_address->set_address(encap_uri.address()->ip()->addressAsString());
  socket_address->set_protocol(is_udp ? envoy::config::core::v3::SocketAddress::UDP : envoy::config::core::v3::SocketAddress::TCP);
  socket_address->set_port_value(encap_uri.port());

  if (is_udp) {
    address->mutable_socket_address()->set_protocol(envoy::config::core::v3::SocketAddress::UDP);
    auto *filter = listener->add_listener_filters();
    filter->set_name("envoy.filters.listener.udp_proxy");
    filter->mutable_typed_config()->set_type_url("type.googleapis.com/envoy.extensions.filters.listener.udp_proxy.v3.UdpProxy");
    envoy::extensions::filters::udp::udp_proxy::v3::UdpProxyConfig udp_proxy_config;
    *udp_proxy_config.mutable_stat_prefix() = "udp_proxy";
    auto *action = udp_proxy_config.mutable_matcher()->mutable_on_no_match()->mutable_action();
    action->set_name("route");
    action->mutable_typed_config()->set_type_url("type.googleapis.com/envoy.extensions.filters.udp.udp_proxy.v3.Route");
    envoy::extensions::filters::udp::udp_proxy::v3::Route route_config;
    route_config.set_cluster("cluster_0");
    action->mutable_typed_config()->PackFrom(route_config);
    
    auto *session_filter = udp_proxy_config.mutable_session_filters()->Add();
    session_filter->set_name("envoy.filters.udp.session.http_capsule");
    session_filter->mutable_typed_config()->set_type_url("type.googleapis.com/envoy.extensions.filters.udp.udp_proxy.session.http_capsule.v3.FilterConfig");
    envoy::extensions::filters::udp::udp_proxy::session::http_capsule::v3::FilterConfig session_filter_config;
    session_filter->mutable_typed_config()->PackFrom(session_filter_config);
    
    auto *tunneling_config = udp_proxy_config.mutable_tunneling_config();
    *tunneling_config->mutable_proxy_host() = "%FILTER_STATE(proxy.host.key:PLAIN)%";
    *tunneling_config->mutable_target_host() = "%FILTER_STATE(target.host.key:PLAIN)%";
    tunneling_config->set_default_target_port(443);
    auto *retry_options = tunneling_config->mutable_retry_options();
    retry_options->mutable_max_connect_attempts()->set_value(2);
    auto *buffer_options = tunneling_config->mutable_buffer_options();
    buffer_options->mutable_max_buffered_datagrams()->set_value(1024);
    buffer_options->mutable_max_buffered_bytes()->set_value(16384);
    auto *headers_to_add = tunneling_config->mutable_headers_to_add()->Add();
    headers_to_add->mutable_header()->set_key("original_dst_port");
    headers_to_add->mutable_header()->set_value("%DOWNSTREAM_LOCAL_PORT%");
    
    filter->mutable_typed_config()->PackFrom(udp_proxy_config);
    
  } else {
    address->mutable_socket_address()->set_protocol(envoy::config::core::v3::SocketAddress::TCP);
    auto *filter = listener->add_filter_chains()->add_filters();
    filter->set_name("envoy.filters.network.tcp_proxy");
    filter->mutable_typed_config()->set_type_url("type.googleapis.com/envoy.extensions.filters.network.tcp_proxy.v3.TcpProxy");
    envoy::extensions::filters::network::tcp_proxy::v3::TcpProxy tcp_proxy_config;
    tcp_proxy_config.set_stat_prefix("tcp_proxy");
    *tcp_proxy_config.mutable_cluster() = "cluster_0";
    auto *tunneling_config = tcp_proxy_config.mutable_tunneling_config();
    *tunneling_config->mutable_hostname() = "host.com:443";
    auto *header_to_add = tunneling_config->add_headers_to_add();
    header_to_add->mutable_header()->set_key("original_dst_port");
    header_to_add->mutable_header()->set_value("%DOWNSTREAM_LOCAL_PORT%");
    filter->mutable_typed_config()->PackFrom(tcp_proxy_config);
  }

  auto *cluster = encap_bootstrap.mutable_static_resources()->add_clusters();
  cluster->set_name("cluster_0");
  cluster->mutable_connect_timeout()->set_seconds(5);

  envoy::extensions::upstreams::http::v3::HttpProtocolOptions protocol_options;
  if(tunnel_protocol == Envoy::Http::Protocol::Http3){
    auto h3_options = protocol_options.mutable_explicit_http_config()->mutable_http3_protocol_options();

    if(options.tunnelHttp3ProtocolOptions().has_value()){
      h3_options->MergeFrom(options.tunnelHttp3ProtocolOptions().value());
    }
    auto *transport_socket = cluster->mutable_transport_socket();
    envoy::extensions::transport_sockets::tls::v3::UpstreamTlsContext upstream_tls_context = options.tunnelTlsContext();
    transport_socket->set_name("envoy.transport_sockets.quic");
    envoy::extensions::transport_sockets::quic::v3::QuicUpstreamTransport quic_upstream_transport;
    *quic_upstream_transport.mutable_upstream_tls_context() = upstream_tls_context;
    transport_socket->mutable_typed_config()->PackFrom(quic_upstream_transport);
    
  }
  else if(tunnel_protocol == Envoy::Http::Protocol::Http2){
      protocol_options.mutable_explicit_http_config()->mutable_http2_protocol_options();
  } else {
      protocol_options.mutable_explicit_http_config()->mutable_http_protocol_options();
  }

  (*cluster->mutable_typed_extension_protocol_options())
  ["envoy.extensions.upstreams.http.v3.HttpProtocolOptions"]
      .PackFrom(protocol_options);


  *cluster->mutable_load_assignment()->mutable_cluster_name() = "cluster_0";
  auto *endpoint = cluster->mutable_load_assignment()->mutable_endpoints()->Add()->add_lb_endpoints()->mutable_endpoint();

  tunnel_uri.resolve(dispatcher, *dns_resolver,
                   Utility::translateFamilyOptionString(options.addressFamily()));

  auto endpoint_socket = endpoint->mutable_address()->mutable_socket_address();
  endpoint_socket->set_address(tunnel_uri.address()->ip()->addressAsString());
  endpoint_socket->set_port_value(tunnel_uri.port());
  
  
  return encap_bootstrap;
}

absl::Status RunWithSubprocess(std::function<void()> nigthawk_fn, std::function<void(sem_t&, sem_t&)> envoy_fn) {
    
  sem_t* envoy_control_sem
  
  = static_cast<sem_t*>(mmap(NULL, sizeof(sem_t), PROT_READ |PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS, -1, 0));
  sem_t* nighthawk_control_sem
  
  = static_cast<sem_t*>(mmap(NULL, sizeof(sem_t), PROT_READ |PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS, -1, 0));

  // create blocked semaphore for envoy
  int ret = sem_init(envoy_control_sem, /*pshared=*/1, /*count=*/0); 
  if (ret != 0) {
    return absl::InternalError("sem_init failed");
  }
  
  // create blocked semaphore for nighthawk
  ret = sem_init(nighthawk_control_sem, /*pshared=*/1, /*count=*/0);
  if (ret != 0) {
    return absl::InternalError("sem_init failed");
  }

  pid_t pid = fork();
  if (pid == -1) {
    return absl::InternalError("fork failed");
  }
  if (pid == 0) {
    envoy_fn(*envoy_control_sem, *nighthawk_control_sem);
    exit(0);
  }
  else{
    // wait for envoy to start and signal nighthawk to start
    sem_wait(nighthawk_control_sem);
    // start nighthawk
    nigthawk_fn();
    // signal envoy to shutdown
    sem_post(envoy_control_sem);
  }
  
  int status;
  waitpid(pid, &status, 0);
  
  sem_destroy(envoy_control_sem);
  munmap(envoy_control_sem, sizeof(sem_t));
  
  sem_destroy(nighthawk_control_sem);
  munmap(nighthawk_control_sem, sizeof(sem_t));
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    // Child process did not crash.
    return absl::OkStatus();
  }
  // Child process crashed.
  return absl::InternalError(absl::StrCat("Execution crashed ", status));
  
}


Envoy::Thread::PosixThreadPtr createThread(std::function<void()> thread_routine) {
  
  Envoy::Thread::Options options;
  
  auto thread_handle =
      new Envoy::Thread::ThreadHandle(thread_routine, options.priority_);
  const int rc =  pthread_create(
    &thread_handle->handle(), nullptr,
    [](void* arg) -> void* {
      auto* handle = static_cast<Envoy::Thread::ThreadHandle*>(arg);
      handle->routine()();
      return nullptr;
    },
    reinterpret_cast<void*>(thread_handle));
  if (rc != 0) {
    delete thread_handle;
    IS_ENVOY_BUG(fmt::format("Unable to create a thread with return code: {}", rc));
    return nullptr;
  }
  return std::make_unique<Envoy::Thread::PosixThread>(thread_handle, options);
}

} // namespace Nighthawk
