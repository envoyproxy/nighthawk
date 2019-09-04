#include "client/benchmark_client_impl.h"

#include "envoy/event/dispatcher.h"
#include "envoy/thread_local/thread_local.h"

#include "nighthawk/common/statistic.h"

#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/http/headers.h"
#include "external/envoy/source/common/http/utility.h"
#include "external/envoy/source/common/network/utility.h"

#include "client/stream_decoder.h"

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
    StatisticPtr&& connect_statistic, StatisticPtr&& response_statistic, UriPtr&& uri, bool use_h2,
    Envoy::Upstream::ClusterManagerPtr& cluster_manager, absl::string_view cluster_name)
    : api_(api), dispatcher_(dispatcher), scope_(scope.createScope("benchmark.")),
      connect_statistic_(std::move(connect_statistic)),
      response_statistic_(std::move(response_statistic)), use_h2_(use_h2), uri_(std::move(uri)),
      benchmark_client_stats_({ALL_BENCHMARK_CLIENT_STATS(POOL_COUNTER(*scope_))}),
      cluster_manager_(cluster_manager), cluster_name_(std::string(cluster_name)) {

  connect_statistic_->setId("benchmark_http_client.queue_to_connect");
  response_statistic_->setId("benchmark_http_client.request_to_response");

  request_headers_.insertMethod().value(Envoy::Http::Headers::get().MethodValues.Get);
  request_headers_.insertPath().value(uri_->path());
  request_headers_.insertHost().value(uri_->hostAndPort());
  request_headers_.insertScheme().value(uri_->scheme() == "https"
                                            ? Envoy::Http::Headers::get().SchemeValues.Https
                                            : Envoy::Http::Headers::get().SchemeValues.Http);
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
    pool()->drainConnections();
  }
  dispatcher_.run(Envoy::Event::Dispatcher::RunType::NonBlock);
  dispatcher_.clearDeferredDeleteList();
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

  auto stream_decoder = new StreamDecoder(dispatcher_, api_.timeSource(), *this,
                                          std::move(caller_completion_callback),
                                          *connect_statistic_, *response_statistic_,
                                          request_headers_, measureLatencies(), request_body_size_);
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