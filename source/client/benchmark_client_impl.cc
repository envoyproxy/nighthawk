#include "client/benchmark_client_impl.h"

#include "envoy/event/dispatcher.h"
#include "envoy/server/tracer_config.h"
#include "envoy/thread_local/thread_local.h"

#include "nighthawk/common/statistic.h"

#include "common/common/compiler_requirements.h"
#include "common/http/header_map_impl.h"
#include "common/http/headers.h"
#include "common/http/http1/conn_pool.h"
#include "common/http/http2/conn_pool.h"
#include "common/http/utility.h"
#include "common/network/dns_impl.h"
#include "common/network/raw_buffer_socket.h"
#include "common/network/utility.h"
#include "common/protobuf/message_validator_impl.h"
#include "common/upstream/cluster_manager_impl.h"
#include "common/upstream/upstream_impl.h"

#include "client/stream_decoder.h"

#include "extensions/transport_sockets/well_known_names.h"

#include "absl/strings/str_split.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

BenchmarkClientHttpImpl::BenchmarkClientHttpImpl(
    Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Store& store,
    StatisticPtr&& connect_statistic, StatisticPtr&& response_statistic, UriPtr&& uri, bool use_h2,
    bool prefetch_connections, envoy::api::v2::auth::UpstreamTlsContext,
    Envoy::Upstream::ClusterManagerPtr& cluster_manager)
    : api_(api), dispatcher_(dispatcher), store_(store),
      scope_(store_.createScope("client.benchmark.")),
      connect_statistic_(std::move(connect_statistic)),
      response_statistic_(std::move(response_statistic)), use_h2_(use_h2),
      prefetch_connections_(prefetch_connections), uri_(std::move(uri)),
      benchmark_client_stats_({ALL_BENCHMARK_CLIENT_STATS(POOL_COUNTER(*scope_))}),
      cluster_manager_(cluster_manager) {

  connect_statistic_->setId("benchmark_http_client.queue_to_connect");
  response_statistic_->setId("benchmark_http_client.request_to_response");

  request_headers_.insertMethod().value(Envoy::Http::Headers::get().MethodValues.Get);
  request_headers_.insertPath().value(uri_->path());
  request_headers_.insertHost().value(uri_->hostAndPort());
  request_headers_.insertScheme().value(uri_->scheme() == "https"
                                            ? Envoy::Http::Headers::get().SchemeValues.Https
                                            : Envoy::Http::Headers::get().SchemeValues.Http);
}

class H1Pool : public PrefetchablePool, public Envoy::Http::Http1::ProdConnPoolImpl {
public:
  H1Pool(Envoy::Event::Dispatcher& dispatcher, Envoy::Upstream::HostConstSharedPtr host,
         Envoy::Upstream::ResourcePriority priority,
         const Envoy::Network::ConnectionSocket::OptionsSharedPtr& options)
      : Envoy::Http::Http1::ProdConnPoolImpl(dispatcher, std::move(host), priority, options) {}

  void prefetchConnections() override {
    while (host_->cluster().resourceManager(priority_).connections().canCreate()) {
      createNewConnection();
    }
  }
  Envoy::Http::ConnectionPool::Instance& pool() override { return *this; }
};

class H2Pool : public PrefetchablePool, public Envoy::Http::Http2::ProdConnPoolImpl {
public:
  H2Pool(Envoy::Event::Dispatcher& dispatcher, Envoy::Upstream::HostConstSharedPtr host,
         Envoy::Upstream::ResourcePriority priority,
         const Envoy::Network::ConnectionSocket::OptionsSharedPtr& options)
      : Envoy::Http::Http2::ProdConnPoolImpl(dispatcher, std::move(host), priority, options) {}

  void prefetchConnections() override {
    // No-op, this is a "pool" with a single connection.
  }
  Envoy::Http::ConnectionPool::Instance& pool() override { return *this; }
};

void BenchmarkClientHttpImpl::prefetchPoolConnections() { /*pool_->prefetchConnections(); */
}

void BenchmarkClientHttpImpl::initialize(Envoy::Runtime::Loader&, Envoy::ThreadLocal::Instance&) {
  cluster_ = cluster_manager_->get("staticcluster")->info();
  auto proto = use_h2_ ? Envoy::Http::Protocol::Http2 : Envoy::Http::Protocol::Http11;
  pool_ = cluster_manager_->httpConnPoolForCluster(
      "staticcluster", Envoy::Upstream::ResourcePriority::Default, proto, nullptr);
}

void BenchmarkClientHttpImpl::terminate() {
  // pool_->drainConnections();
  // cluster_manager_.reset();
  pool_ = nullptr;
  /* pool_.reset();*/
}

StatisticPtrMap BenchmarkClientHttpImpl::statistics() const {
  StatisticPtrMap statistics;
  statistics[connect_statistic_->id()] = connect_statistic_.get();
  statistics[response_statistic_->id()] = response_statistic_.get();
  return statistics;
};

void BenchmarkClientHttpImpl::setRequestHeader(absl::string_view key, absl::string_view value) {
  auto lower_case_key = Envoy::Http::LowerCaseString(std::string(key));
  request_headers_.remove(lower_case_key);
  // TODO(oschaaf): we've performed zero validation on the header key/value.
  request_headers_.addCopy(lower_case_key, std::string(value));
}

bool BenchmarkClientHttpImpl::tryStartOne(std::function<void()> caller_completion_callback) {
  // When we allow client-side queuing, we want to have a sense of time spend waiting on that queue.
  // So we return false here to indicate we couldn't initiate a new request.
  if (!cluster_->resourceManager(Envoy::Upstream::ResourcePriority::Default)
           .pendingRequests()
           .canCreate()) {
    return false;
  }
  // When no client side queueing is disabled (max_pending equals 1) we control the pacing as
  // exactly as possible here.
  // NOTE: We can't consistently rely on resourceManager()::requests()
  // because that isn't used for h/1 (it is used in tcp and h2 though).
  if ((max_pending_requests_ == 1 &&
       (requests_initiated_ - requests_completed_) >= connection_limit_)) {
    return false;
  }

  auto stream_decoder = new StreamDecoder(dispatcher_, api_.timeSource(), *this,
                                          std::move(caller_completion_callback),
                                          *connect_statistic_, *response_statistic_,
                                          request_headers_, measureLatencies(), request_body_size_);
  requests_initiated_++;
  if (prefetch_connections_) {
  }
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
  switch (reason) {
  case Envoy::Http::ConnectionPool::PoolFailureReason::Overflow:
    benchmark_client_stats_.pool_overflow_.inc();
    break;
  case Envoy::Http::ConnectionPool::PoolFailureReason::ConnectionFailure:
    benchmark_client_stats_.pool_connection_failure_.inc();
    break;
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

} // namespace Client
} // namespace Nighthawk