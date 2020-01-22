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

Envoy::Http::ConnectionPool::Cancellable*
Http1PoolImpl::newStream(Envoy::Http::StreamDecoder& response_decoder,
                         Envoy::Http::ConnectionPool::Callbacks& callbacks) {
  // In prefetch mode we try to keep the amount of connections at the configured limit.
  if (prefetch_connections_) {
    while (host_->cluster().resourceManager(priority_).connections().canCreate()) {
      createNewConnection();
    }
  }

  // By default, Envoy re-uses the most recent free connection. Here we pop from the back
  // of ready_clients_, which will pick the oldest one instead. This makes us cycle through
  // all the available connections.
  if (!ready_clients_.empty() && connection_reuse_strategy_ == ConnectionReuseStrategy::LRU) {
    ready_clients_.back()->moveBetweenLists(ready_clients_, busy_clients_);
    attachRequestToClient(*busy_clients_.front(), response_decoder, callbacks);
    return nullptr;
  }

  // Vanilla Envoy pool behavior.
  return ConnPoolImpl::newStream(response_decoder, callbacks);
}

Http2PoolImpl::Http2PoolImpl(
    Envoy::Event::Dispatcher& dispatcher, Envoy::Upstream::HostConstSharedPtr host,
    Envoy::Upstream::ResourcePriority priority,
    const Envoy::Network::ConnectionSocket::OptionsSharedPtr& options,               // NOLINT
    const Envoy::Network::TransportSocketOptionsSharedPtr& transport_socket_options) // NOLINT
    : Envoy::Http::ConnPoolImplBase(std::move(host), priority), dispatcher_(dispatcher),
      socket_options_(options), transport_socket_options_(transport_socket_options) {
  for (uint32_t i = 0; i < host_->cluster().resourceManager(priority_).connections().max(); i++) {
    pools_.push_back(std::make_unique<Envoy::Http::Http2::ProdConnPoolImpl>(
        dispatcher_, host_, priority_, socket_options_, transport_socket_options_));
  }
}

void Http2PoolImpl::addDrainedCallback(Envoy::Http::ConnectionPool::Instance::DrainedCb cb) {
  for (auto& pool : pools_) {
    pool->addDrainedCallback(cb);
  }
}

void Http2PoolImpl::drainConnections() {
  for (auto& pool : pools_) {
    pool->drainConnections();
  }
}

bool Http2PoolImpl::hasActiveConnections() const {
  for (auto& pool : pools_) {
    if (pool->hasActiveConnections()) {
      return true;
    }
  }
  return false;
}

Envoy::Http::ConnectionPool::Cancellable*
Http2PoolImpl::newStream(Envoy::Http::StreamDecoder& response_decoder,
                         Envoy::Http::ConnectionPool::Callbacks& callbacks) {
  // Use the simplest but probably naive approach of rotating over the available pool instances
  // / connections to distribute requests accross them.
  return pools_[pool_round_robin_index_++ % pools_.size()]->newStream(response_decoder, callbacks);
};

void Http2PoolImpl::checkForDrained() {
  // TODO(oschaaf): this one is protected, can't forward it.
}

BenchmarkClientHttpImpl::BenchmarkClientHttpImpl(
    Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
    StatisticPtr&& connect_statistic, StatisticPtr&& response_statistic,
    StatisticPtr&& response_header_size_statistic, StatisticPtr&& response_body_size_statistic,
    bool use_h2, Envoy::Upstream::ClusterManagerPtr& cluster_manager,
    Envoy::Tracing::HttpTracerPtr& http_tracer, absl::string_view cluster_name,
    RequestGenerator request_generator, const bool provide_resource_backpressure)
    : api_(api), dispatcher_(dispatcher), scope_(scope.createScope("benchmark.")),
      connect_statistic_(std::move(connect_statistic)),
      response_statistic_(std::move(response_statistic)),
      response_header_size_statistic_(std::move(response_header_size_statistic)),
      response_body_size_statistic_(std::move(response_body_size_statistic)), use_h2_(use_h2),
      benchmark_client_stats_({ALL_BENCHMARK_CLIENT_STATS(POOL_COUNTER(*scope_))}),
      cluster_manager_(cluster_manager), http_tracer_(http_tracer),
      cluster_name_(std::string(cluster_name)), request_generator_(std::move(request_generator)),
      provide_resource_backpressure_(provide_resource_backpressure) {
  connect_statistic_->setId("benchmark_http_client.queue_to_connect");
  response_statistic_->setId("benchmark_http_client.request_to_response");
  response_header_size_statistic_->setId("benchmark_http_client.response_header_size");
  response_body_size_statistic_->setId("benchmark_http_client.response_body_size");
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
  statistics[response_header_size_statistic_->id()] = response_header_size_statistic_.get();
  statistics[response_body_size_statistic_->id()] = response_body_size_statistic_.get();
  return statistics;
};

bool BenchmarkClientHttpImpl::tryStartRequest(CompletionCallback caller_completion_callback) {
  auto* pool_ptr = pool();
  if (pool_ptr == nullptr) {
    return false;
  }
  if (provide_resource_backpressure_) {
    const uint64_t max_in_flight =
        max_pending_requests_ + (use_h2_ ? max_active_requests_ : connection_limit_);

    if (requests_initiated_ - requests_completed_ >= max_in_flight) {
      // When we allow client-side queueing, we want to have a sense of time spend waiting on that
      // queue. So we return false here to indicate we couldn't initiate a new request.
      return false;
    }
  }
  auto request = request_generator_();
  // The header generator may not have something for us to send. We'll try next time.
  // TODO(oschaaf): track occurrences of this via a counter & consider setting up a default failure
  // condition for when this happens.
  if (request == nullptr) {
    return false;
  }
  auto* content_length_header = request->header()->ContentLength();
  uint64_t content_length = 0;
  if (content_length_header != nullptr) {
    auto s_content_length = content_length_header->value().getStringView();
    if (!absl::SimpleAtoi(s_content_length, &content_length)) {
      ENVOY_LOG(error, "Ignoring bad content length of {}", s_content_length);
      content_length = 0;
    }
  }

  std::string x_request_id = generator_.uuid();
  auto stream_decoder = new StreamDecoder(
      dispatcher_, api_.timeSource(), *this, std::move(caller_completion_callback),
      *connect_statistic_, *response_statistic_, *response_header_size_statistic_,
      *response_body_size_statistic_, request->header(), shouldMeasureLatencies(), content_length,
      x_request_id, http_tracer_);
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