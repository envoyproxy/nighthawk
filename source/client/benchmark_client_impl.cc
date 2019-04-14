#include "client/benchmark_client_impl.h"

#include "absl/strings/str_split.h"

#include "envoy/event/dispatcher.h"
#include "envoy/thread_local/thread_local.h"

#include "extensions/transport_sockets/well_known_names.h"

#include "common/common/compiler_requirements.h"
#include "common/http/header_map_impl.h"
#include "common/http/headers.h"
#include "common/http/http1/conn_pool.h"
#include "common/http/http2/conn_pool.h"
#include "common/http/utility.h"
#include "common/network/dns_impl.h"
#include "common/network/raw_buffer_socket.h"
#include "common/network/utility.h"
#include "common/upstream/cluster_manager_impl.h"
#include "common/upstream/upstream_impl.h"

#include "nighthawk/common/statistic.h"

#include "client/stream_decoder.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

BenchmarkClientHttpImpl::BenchmarkClientHttpImpl(
    Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Store& store,
    StatisticPtr&& connect_statistic, StatisticPtr&& response_statistic, UriPtr&& uri, bool use_h2)
    : api_(api), dispatcher_(dispatcher), store_(store),
      scope_(store_.createScope("client.benchmark.")),
      connect_statistic_(std::move(connect_statistic)),
      response_statistic_(std::move(response_statistic)), use_h2_(use_h2), uri_(std::move(uri)),
      benchmark_client_stats_({ALL_BENCHMARK_CLIENT_STATS(POOL_COUNTER(*scope_))}) {
  connect_statistic_->setId("benchmark_http_client.queue_to_connect");
  response_statistic_->setId("benchmark_http_client.request_to_response");

  request_headers_.insertMethod().value(Envoy::Http::Headers::get().MethodValues.Get);
  request_headers_.insertPath().value(uri_->path());
  request_headers_.insertHost().value(uri_->host_and_port());
  request_headers_.insertScheme().value(uri_->scheme() == "https"
                                            ? Envoy::Http::Headers::get().SchemeValues.Https
                                            : Envoy::Http::Headers::get().SchemeValues.Http);
}

void BenchmarkClientHttpImpl::initialize(Envoy::Runtime::Loader& runtime) {
  ASSERT(uri_->address() != nullptr);
  envoy::api::v2::Cluster cluster_config;
  envoy::api::v2::core::BindConfig bind_config;

  cluster_config.mutable_connect_timeout()->set_seconds(timeout_.count());
  auto thresholds = cluster_config.mutable_circuit_breakers()->add_thresholds();

  thresholds->mutable_max_retries()->set_value(0);
  thresholds->mutable_max_connections()->set_value(connection_limit_);
  thresholds->mutable_max_pending_requests()->set_value(max_pending_requests_);

  Envoy::Network::TransportSocketFactoryPtr socket_factory;

  if (uri_->scheme() == "https") {
    auto common_tls_context = cluster_config.mutable_tls_context()->mutable_common_tls_context();
    // TODO(oschaaf): we should ensure that we fail when h2 is requested but not supported on the
    // server-side in tests.
    if (use_h2_) {
      common_tls_context->add_alpn_protocols("h2");
    } else {
      common_tls_context->add_alpn_protocols("http/1.1");
    }
    auto transport_socket = cluster_config.transport_socket();
    if (!cluster_config.has_transport_socket()) {
      ASSERT(cluster_config.has_tls_context());
      transport_socket.set_name(
          Envoy::Extensions::TransportSockets::TransportSocketNames::get().Tls);
      transport_socket.mutable_typed_config()->PackFrom(cluster_config.tls_context());
    }

    // TODO(oschaaf): Ideally we'd just re-use Tls::Upstream::createTransportFactory().
    // But instead of doing that, we need to perform some of what that implements ourselves here,
    // so we can skip message validation which may trigger an assert in integration tests.
    auto& config_factory = Envoy::Config::Utility::getAndCheckFactory<
        Envoy::Server::Configuration::UpstreamTransportSocketConfigFactory>(
        transport_socket.name());
    Envoy::ProtobufTypes::MessagePtr message =
        Envoy::Config::Utility::translateToFactoryConfig(transport_socket, config_factory);

    ssl_context_manager_ =
        std::make_unique<Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl>(
            api_.timeSource());
    transport_socket_factory_context_ = std::make_unique<Ssl::MinimalTransportSocketFactoryContext>(
        store_.createScope("client."), dispatcher_, generator_, store_, api_,
        *ssl_context_manager_);

    auto client_config =
        std::make_unique<Envoy::Extensions::TransportSockets::Tls::ClientContextConfigImpl>(
            dynamic_cast<const envoy::api::v2::auth::UpstreamTlsContext&>(*message),
            *transport_socket_factory_context_);
    socket_factory =
        std::make_unique<Envoy::Extensions::TransportSockets::Tls::ClientSslSocketFactory>(
            std::move(client_config), transport_socket_factory_context_->sslContextManager(),
            transport_socket_factory_context_->statsScope());
  } else {
    socket_factory = std::make_unique<Envoy::Network::RawBufferSocketFactory>();
  };

  cluster_ = std::make_unique<Envoy::Upstream::ClusterInfoImpl>(
      cluster_config, bind_config, runtime, std::move(socket_factory),
      store_.createScope("client."), false /*added_via_api*/);

  ASSERT(uri_->address() != nullptr);

  auto host = std::shared_ptr<Envoy::Upstream::Host>{new Envoy::Upstream::HostImpl(
      cluster_, std::string(uri_->host_and_port()), uri_->address(),
      envoy::api::v2::core::Metadata::default_instance(), 1 /* weight */,
      envoy::api::v2::core::Locality(),
      envoy::api::v2::endpoint::Endpoint::HealthCheckConfig::default_instance(), 0,
      envoy::api::v2::core::HealthStatus::HEALTHY)};

  Envoy::Network::ConnectionSocket::OptionsSharedPtr options =
      std::make_shared<Envoy::Network::ConnectionSocket::Options>();
  if (use_h2_) {
    pool_ = std::make_unique<Envoy::Http::Http2::ProdConnPoolImpl>(
        dispatcher_, host, Envoy::Upstream::ResourcePriority::Default, options);
  } else {
    pool_ = std::make_unique<Envoy::Http::Http1::ProdConnPoolImpl>(
        dispatcher_, host, Envoy::Upstream::ResourcePriority::Default, options);
  }
}

void BenchmarkClientHttpImpl::terminate() { pool_.reset(); }

StatisticPtrMap BenchmarkClientHttpImpl::statistics() const {
  StatisticPtrMap statistics;
  statistics[connect_statistic_->id()] = connect_statistic_.get();
  statistics[response_statistic_->id()] = response_statistic_.get();
  return statistics;
};

bool BenchmarkClientHttpImpl::tryStartOne(std::function<void()> caller_completion_callback) {
  if (!cluster_->resourceManager(Envoy::Upstream::ResourcePriority::Default)
           .pendingRequests()
           .canCreate() ||
      // In closed loop mode we want to be able to control the pacing as exactly as possible.
      // In open-loop mode we probably want to skip this.
      // NOTE(oschaaf): We can't consistently rely on resourceManager()::requests() because that
      // isn't used for h/1 (it is used in tcp and h2 though).
      ((requests_initiated_ - requests_completed_) >= connection_limit_)) {
    return false;
  }

  auto stream_decoder = new StreamDecoder(
      dispatcher_, api_.timeSource(), *this, std::move(caller_completion_callback),
      *connect_statistic_, *response_statistic_, request_headers_, measureLatencies());
  requests_initiated_++;
  pool_->newStream(*stream_decoder, *stream_decoder);
  return true;
}

void BenchmarkClientHttpImpl::onComplete(bool success, const Envoy::Http::HeaderMap& headers) {
  requests_completed_++;
  if (!success) {
    benchmark_client_stats_.stream_resets_.inc();
  } else {
    ASSERT(headers.Status());
    const int64_t status = Envoy::Http::Utility::getResponseStatus(headers);

    if (status > 99 && status <= 199) {
      benchmark_client_stats_.http_1xx_.inc();
    } else if (status > 199 && status <= 299) {
      benchmark_client_stats_.http_2xx_.inc();
    } else if (status > 299 && status <= 399) {
      benchmark_client_stats_.http_3xx_.inc();
    } else if (status > 399 && status <= 499) {
      benchmark_client_stats_.http_4xx_.inc();
    } else if (status > 499 && status <= 599) {
      benchmark_client_stats_.http_5xx_.inc();
    } else {
      benchmark_client_stats_.http_xxx_.inc();
    }
  }
}

void BenchmarkClientHttpImpl::onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason) {
  ASSERT(reason == Envoy::Http::ConnectionPool::PoolFailureReason::ConnectionFailure);
}

} // namespace Client
} // namespace Nighthawk