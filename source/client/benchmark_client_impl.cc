#include "client/benchmark_client_impl.h"

#include "envoy/event/dispatcher.h"
#include "envoy/thread_local/thread_local.h"

#include "nighthawk/common/statistic.h"

#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/http/headers.h"
#include "external/envoy/source/common/http/utility.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/runtime/uuid_util.h"

#include "client/stream_decoder.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

void Http1PoolImpl::createConnections(const uint32_t connection_limit) {
  ENVOY_LOG(error, "Prefetching {} connections.", connection_limit);
  for (uint32_t i = 0; i < connection_limit; i++) {
    createNewConnection();
  }
}

BenchmarkClientHttpImpl::BenchmarkClientHttpImpl(
    Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
    StatisticPtr&& connect_statistic, StatisticPtr&& response_statistic, bool use_h2,
    Envoy::Upstream::ClusterManagerPtr& cluster_manager, Envoy::Tracing::HttpTracerPtr& http_tracer,
    absl::string_view cluster_name, RequestGenerator request_generator)
    : api_(api), dispatcher_(dispatcher), scope_(scope.createScope("benchmark.")),
      connect_statistic_(std::move(connect_statistic)),
      response_statistic_(std::move(response_statistic)), use_h2_(use_h2),
      benchmark_client_stats_({ALL_BENCHMARK_CLIENT_STATS(POOL_COUNTER(*scope_))}),
      cluster_manager_(cluster_manager), http_tracer_(http_tracer),
      cluster_name_(std::string(cluster_name)), request_generator_(std::move(request_generator)) {
  connect_statistic_->setId("benchmark_http_client.queue_to_connect");
  response_statistic_->setId("benchmark_http_client.request_to_response");
}

void BenchmarkClientHttpImpl::prefetchPoolConnections() {
  auto* prefetchable_pool = dynamic_cast<Http1PoolImpl*>(pool());
  if (prefetchable_pool == nullptr) {
    ENVOY_LOG(error, "prefetchPoolConnections() pool not prefetchable");
    return;
  }
  prefetchable_pool->createConnections(connection_limit_);
}

void BenchmarkClientHttpImpl::terminate() {
  if (pool() != nullptr) {
    pool()->addDrainedCallback([this]() -> void { dispatcher_.exit(); });
    pool()->drainConnections();
    dispatcher_.run(Envoy::Event::Dispatcher::RunType::RunUntilExit);
  }
}

StatisticPtrMap BenchmarkClientHttpImpl::statistics() const {
  StatisticPtrMap statistics;
  statistics[connect_statistic_->id()] = connect_statistic_.get();
  statistics[response_statistic_->id()] = response_statistic_.get();
  return statistics;
};

bool BenchmarkClientHttpImpl::tryStartRequest(CompletionCallback caller_completion_callback) {
  // When we allow client-side queuing, we want to have a sense of time spend waiting on that queue.
  // So we return false here to indicate we couldn't initiate a new request.
  auto* pool_ptr = pool();
  auto cluster_info = cluster();
  if (pool_ptr == nullptr || cluster_info == nullptr ||
      !cluster_info->resourceManager(Envoy::Upstream::ResourcePriority::Default)
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
  auto header = request_generator_();
  // The header generator may not have something for us to send, which is OK.
  // We'll try next time.
  if (header == nullptr) {
    return false;
  }
  auto* content_length_header = header->header()->ContentLength();
  uint64_t content_length = 0;
  if (content_length_header != nullptr) {
    auto s_content_length = header->header()->ContentLength()->value().getStringView();
    if (!absl::SimpleAtoi(s_content_length, &content_length)) {
      ENVOY_LOG(error, "Ignoring bad content length of {}", s_content_length);
      content_length = 0;
    }
  }

  std::string x_request_id = generator_.uuid();
  auto stream_decoder = new StreamDecoder(
      dispatcher_, api_.timeSource(), *this, std::move(caller_completion_callback),
      *connect_statistic_, *response_statistic_, header->header(), measureLatencies(),
      content_length, x_request_id, http_tracer_);
  requests_initiated_++;
  pool_ptr->newStream(*stream_decoder, *stream_decoder);
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