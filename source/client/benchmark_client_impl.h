#pragma once

#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/http/conn_pool.h"
#include "envoy/network/address.h"
#include "envoy/runtime/runtime.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/store.h"
#include "envoy/upstream/upstream.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/statistic.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/http/http1/conn_pool.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"

#include "api/client/options.pb.h"

#include "client/stream_decoder.h"

namespace Nighthawk {
namespace Client {

using namespace std::chrono_literals;

using namespace Envoy; // We need this because of macro expectations.

#define ALL_BENCHMARK_CLIENT_STATS(COUNTER)                                                        \
  COUNTER(stream_resets)                                                                           \
  COUNTER(http_1xx)                                                                                \
  COUNTER(http_2xx)                                                                                \
  COUNTER(http_3xx)                                                                                \
  COUNTER(http_4xx)                                                                                \
  COUNTER(http_5xx)                                                                                \
  COUNTER(http_xxx)                                                                                \
  COUNTER(pool_overflow)                                                                           \
  COUNTER(pool_connection_failure)

struct BenchmarkClientStats {
  ALL_BENCHMARK_CLIENT_STATS(GENERATE_COUNTER_STRUCT)
};

class Http1PoolImpl : public Envoy::Http::Http1::ProdConnPoolImpl {
public:
  using Envoy::Http::Http1::ProdConnPoolImpl::ProdConnPoolImpl;
  void createConnections(const uint32_t connection_limit);
};

class BenchmarkClientHttpImpl : public BenchmarkClient,
                                public StreamDecoderCompletionCallback,
                                public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  BenchmarkClientHttpImpl(Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher,
                          Envoy::Stats::Scope& scope, StatisticPtr&& connect_statistic,
                          StatisticPtr&& response_statistic, bool use_h2,
                          Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                          Envoy::Tracing::HttpTracerPtr& http_tracer,
                          absl::string_view cluster_name, RequestGenerator request_generator);

  void setConnectionLimit(uint32_t connection_limit) { connection_limit_ = connection_limit; }
  void setMaxPendingRequests(uint32_t max_pending_requests) {
    max_pending_requests_ = max_pending_requests;
  }
  void setMaxActiveRequests(uint32_t max_active_requests) {
    max_active_requests_ = max_active_requests;
  }
  void setMaxRequestsPerConnection(uint32_t max_requests_per_connection) {
    max_requests_per_connection_ = max_requests_per_connection;
  }

  // BenchmarkClient
  void terminate() override;
  StatisticPtrMap statistics() const override;
  bool measureLatencies() const override { return measure_latencies_; }
  void setMeasureLatencies(bool measure_latencies) override {
    measure_latencies_ = measure_latencies;
  }
  bool tryStartRequest(CompletionCallback caller_completion_callback) override;
  Envoy::Stats::Scope& scope() const override { return *scope_; }

  // StreamDecoderCompletionCallback
  void onComplete(bool success, const Envoy::Http::HeaderMap& headers) override;
  void onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason) override;

  // Helpers
  Envoy::Http::ConnectionPool::Instance* pool() {
    auto proto = use_h2_ ? Envoy::Http::Protocol::Http2 : Envoy::Http::Protocol::Http11;
    return cluster_manager_->httpConnPoolForCluster(
        cluster_name_, Envoy::Upstream::ResourcePriority::Default, proto, nullptr);
  }

  Envoy::Upstream::ClusterInfoConstSharedPtr cluster() {
    auto* cluster = cluster_manager_->get(cluster_name_);
    return cluster == nullptr ? nullptr : cluster->info();
  }

  void prefetchPoolConnections() override;

private:
  Envoy::Api::Api& api_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::Stats::ScopePtr scope_;
  // These are declared order dependent. Changing ordering may trigger on assert upon
  // destruction when tls has been involved during usage.
  StatisticPtr connect_statistic_;
  StatisticPtr response_statistic_;
  const bool use_h2_;
  std::chrono::seconds timeout_{5s};
  uint32_t connection_limit_{1};
  uint32_t max_pending_requests_{1};
  uint32_t max_active_requests_{UINT32_MAX};
  uint32_t max_requests_per_connection_{UINT32_MAX};
  Envoy::Event::TimerPtr timer_;
  Envoy::Runtime::RandomGeneratorImpl generator_;
  uint64_t requests_completed_{};
  uint64_t requests_initiated_{};
  bool measure_latencies_{};
  BenchmarkClientStats benchmark_client_stats_;
  Envoy::Upstream::ClusterManagerPtr& cluster_manager_;
  Envoy::Tracing::HttpTracerPtr& http_tracer_;
  std::string cluster_name_;
  const RequestGenerator request_generator_;
};

} // namespace Client
} // namespace Nighthawk