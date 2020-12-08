#include "client/benchmark_client_impl.h"

#include "envoy/event/dispatcher.h"
#include "envoy/thread_local/thread_local.h"

#include "nighthawk/common/statistic.h"

#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/http/headers.h"
#include "external/envoy/source/common/http/utility.h"
#include "external/envoy/source/common/network/utility.h"

#include "client/stream_decoder.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

BenchmarkClientStatistic::BenchmarkClientStatistic(BenchmarkClientStatistic&& statistic) noexcept
    : connect_statistic(std::move(statistic.connect_statistic)),
      response_statistic(std::move(statistic.response_statistic)),
      response_header_size_statistic(std::move(statistic.response_header_size_statistic)),
      response_body_size_statistic(std::move(statistic.response_body_size_statistic)),
      latency_1xx_statistic(std::move(statistic.latency_1xx_statistic)),
      latency_2xx_statistic(std::move(statistic.latency_2xx_statistic)),
      latency_3xx_statistic(std::move(statistic.latency_3xx_statistic)),
      latency_4xx_statistic(std::move(statistic.latency_4xx_statistic)),
      latency_5xx_statistic(std::move(statistic.latency_5xx_statistic)),
      latency_xxx_statistic(std::move(statistic.latency_xxx_statistic)),
      origin_latency_statistic(std::move(statistic.origin_latency_statistic)) {}

BenchmarkClientStatistic::BenchmarkClientStatistic(
    StatisticPtr&& connect_stat, StatisticPtr&& response_stat,
    StatisticPtr&& response_header_size_stat, StatisticPtr&& response_body_size_stat,
    StatisticPtr&& latency_1xx_stat, StatisticPtr&& latency_2xx_stat,
    StatisticPtr&& latency_3xx_stat, StatisticPtr&& latency_4xx_stat,
    StatisticPtr&& latency_5xx_stat, StatisticPtr&& latency_xxx_stat,
    StatisticPtr&& origin_latency_stat)
    : connect_statistic(std::move(connect_stat)), response_statistic(std::move(response_stat)),
      response_header_size_statistic(std::move(response_header_size_stat)),
      response_body_size_statistic(std::move(response_body_size_stat)),
      latency_1xx_statistic(std::move(latency_1xx_stat)),
      latency_2xx_statistic(std::move(latency_2xx_stat)),
      latency_3xx_statistic(std::move(latency_3xx_stat)),
      latency_4xx_statistic(std::move(latency_4xx_stat)),
      latency_5xx_statistic(std::move(latency_5xx_stat)),
      latency_xxx_statistic(std::move(latency_xxx_stat)),
      origin_latency_statistic(std::move(origin_latency_stat)) {}

Envoy::Http::ConnectionPool::Cancellable*
Http1PoolImpl::newStream(Envoy::Http::ResponseDecoder& response_decoder,
                         Envoy::Http::ConnectionPool::Callbacks& callbacks) {
  // In prefetch mode we try to keep the amount of connections at the configured limit.
  if (prefetch_connections_) {
    while (host_->cluster().resourceManager(priority_).connections().canCreate()) {
      // We cannot rely on ::tryCreateConnection here, because that might decline without
      // updating connections().canCreate() above. We would risk an infinite loop.
      Envoy::ConnectionPool::ActiveClientPtr client = instantiateActiveClient();
      connecting_stream_capacity_ += client->effectiveConcurrentStreamLimit();
      Envoy::LinkedList::moveIntoList(std::move(client), owningList(client->state_));
    }
  }

  // By default, Envoy re-uses the most recent free connection. Here we pop from the back
  // of ready_clients_, which will pick the oldest one instead. This makes us cycle through
  // all the available connections.
  if (!ready_clients_.empty() && connection_reuse_strategy_ == ConnectionReuseStrategy::LRU) {
    Envoy::Http::HttpAttachContext context({&response_decoder, &callbacks});
    attachStreamToClient(*ready_clients_.back(), context);
    return nullptr;
  }

  // Vanilla Envoy pool behavior.
  return HttpConnPoolImplBase::newStream(response_decoder, callbacks);
}

BenchmarkClientHttpImpl::BenchmarkClientHttpImpl(
    Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
    BenchmarkClientStatistic& statistic, bool use_h2,
    Envoy::Upstream::ClusterManagerPtr& cluster_manager,
    Envoy::Tracing::HttpTracerSharedPtr& http_tracer, absl::string_view cluster_name,
    RequestGenerator request_generator, const bool provide_resource_backpressure,
    absl::string_view latency_response_header_name)
    : api_(api), dispatcher_(dispatcher), scope_(scope.createScope("benchmark.")),
      statistic_(std::move(statistic)), use_h2_(use_h2),
      benchmark_client_counters_({ALL_BENCHMARK_CLIENT_COUNTERS(POOL_COUNTER(*scope_))}),
      cluster_manager_(cluster_manager), http_tracer_(http_tracer),
      cluster_name_(std::string(cluster_name)), request_generator_(std::move(request_generator)),
      provide_resource_backpressure_(provide_resource_backpressure),
      latency_response_header_name_(latency_response_header_name) {
  statistic_.connect_statistic->setId("benchmark_http_client.queue_to_connect");
  statistic_.response_statistic->setId("benchmark_http_client.request_to_response");
  statistic_.response_header_size_statistic->setId("benchmark_http_client.response_header_size");
  statistic_.response_body_size_statistic->setId("benchmark_http_client.response_body_size");
  statistic_.latency_1xx_statistic->setId("benchmark_http_client.latency_1xx");
  statistic_.latency_2xx_statistic->setId("benchmark_http_client.latency_2xx");
  statistic_.latency_3xx_statistic->setId("benchmark_http_client.latency_3xx");
  statistic_.latency_4xx_statistic->setId("benchmark_http_client.latency_4xx");
  statistic_.latency_5xx_statistic->setId("benchmark_http_client.latency_5xx");
  statistic_.latency_xxx_statistic->setId("benchmark_http_client.latency_xxx");
  statistic_.origin_latency_statistic->setId("benchmark_http_client.origin_latency_statistic");
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
  statistics[statistic_.connect_statistic->id()] = statistic_.connect_statistic.get();
  statistics[statistic_.response_statistic->id()] = statistic_.response_statistic.get();
  statistics[statistic_.response_header_size_statistic->id()] =
      statistic_.response_header_size_statistic.get();
  statistics[statistic_.response_body_size_statistic->id()] =
      statistic_.response_body_size_statistic.get();
  statistics[statistic_.latency_1xx_statistic->id()] = statistic_.latency_1xx_statistic.get();
  statistics[statistic_.latency_2xx_statistic->id()] = statistic_.latency_2xx_statistic.get();
  statistics[statistic_.latency_3xx_statistic->id()] = statistic_.latency_3xx_statistic.get();
  statistics[statistic_.latency_4xx_statistic->id()] = statistic_.latency_4xx_statistic.get();
  statistics[statistic_.latency_5xx_statistic->id()] = statistic_.latency_5xx_statistic.get();
  statistics[statistic_.latency_xxx_statistic->id()] = statistic_.latency_xxx_statistic.get();
  statistics[statistic_.origin_latency_statistic->id()] = statistic_.origin_latency_statistic.get();
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
      ENVOY_LOG_EVERY_POW_2(error, "Ignoring bad content length of {}", s_content_length);
      content_length = 0;
    }
  }

  auto stream_decoder = new StreamDecoder(
      dispatcher_, api_.timeSource(), *this, std::move(caller_completion_callback),
      *statistic_.connect_statistic, *statistic_.response_statistic,
      *statistic_.response_header_size_statistic, *statistic_.response_body_size_statistic,
      *statistic_.origin_latency_statistic, request->header(), shouldMeasureLatencies(),
      content_length, generator_, http_tracer_, latency_response_header_name_);
  requests_initiated_++;
  pool_ptr->newStream(*stream_decoder, *stream_decoder);
  return true;
}

void BenchmarkClientHttpImpl::onComplete(bool success,
                                         const Envoy::Http::ResponseHeaderMap& headers) {
  requests_completed_++;
  if (!success) {
    benchmark_client_counters_.stream_resets_.inc();
  } else {
    ASSERT(headers.Status());
    const int64_t status = Envoy::Http::Utility::getResponseStatus(headers);

    if (status > 99 && status <= 199) {
      benchmark_client_counters_.http_1xx_.inc();
    } else if (status > 199 && status <= 299) {
      benchmark_client_counters_.http_2xx_.inc();
    } else if (status > 299 && status <= 399) {
      benchmark_client_counters_.http_3xx_.inc();
    } else if (status > 399 && status <= 499) {
      benchmark_client_counters_.http_4xx_.inc();
    } else if (status > 499 && status <= 599) {
      benchmark_client_counters_.http_5xx_.inc();
    } else {
      benchmark_client_counters_.http_xxx_.inc();
    }
  }
}

void BenchmarkClientHttpImpl::onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason) {
  switch (reason) {
  case Envoy::Http::ConnectionPool::PoolFailureReason::Overflow:
    benchmark_client_counters_.pool_overflow_.inc();
    break;
  case Envoy::Http::ConnectionPool::PoolFailureReason::LocalConnectionFailure:
  case Envoy::Http::ConnectionPool::PoolFailureReason::RemoteConnectionFailure:
    benchmark_client_counters_.pool_connection_failure_.inc();
    break;
  case Envoy::Http::ConnectionPool::PoolFailureReason::Timeout:
    break;
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

void BenchmarkClientHttpImpl::exportLatency(const uint32_t response_code,
                                            const uint64_t latency_ns) {
  if (response_code > 99 && response_code <= 199) {
    statistic_.latency_1xx_statistic->addValue(latency_ns);
  } else if (response_code > 199 && response_code <= 299) {
    statistic_.latency_2xx_statistic->addValue(latency_ns);
  } else if (response_code > 299 && response_code <= 399) {
    statistic_.latency_3xx_statistic->addValue(latency_ns);
  } else if (response_code > 399 && response_code <= 499) {
    statistic_.latency_4xx_statistic->addValue(latency_ns);
  } else if (response_code > 499 && response_code <= 599) {
    statistic_.latency_5xx_statistic->addValue(latency_ns);
  } else {
    statistic_.latency_xxx_statistic->addValue(latency_ns);
  }
}

} // namespace Client
} // namespace Nighthawk
