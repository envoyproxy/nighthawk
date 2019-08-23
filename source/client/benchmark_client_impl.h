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
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/http/header_map_impl.h"
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
                          Envoy::Stats::Store& store, StatisticPtr&& connect_statistic,
                          StatisticPtr&& response_statistic, UriPtr&& uri, bool use_h2,
                          Envoy::Upstream::ClusterManagerPtr& cluster_manager);

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
  bool tryStartOne(std::function<void()> caller_completion_callback) override;
  Envoy::Stats::Store& store() const override { return store_; }

  void setRequestMethod(envoy::api::v2::core::RequestMethod request_method) override {
    request_headers_.insertMethod().value(envoy::api::v2::core::RequestMethod_Name(request_method));
  };
  void setRequestHeader(absl::string_view key, absl::string_view value) override;
  void setRequestBodySize(uint32_t request_body_size) override {
    request_body_size_ = request_body_size;
  };
  const Envoy::Http::HeaderMap& requestHeaders() const override { return request_headers_; }

  // StreamDecoderCompletionCallback
  void onComplete(bool success, const Envoy::Http::HeaderMap& headers) override;
  void onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason) override;

  // Helpers
  Envoy::Http::ConnectionPool::Instance* pool() {
    auto proto = use_h2_ ? Envoy::Http::Protocol::Http2 : Envoy::Http::Protocol::Http11;
    return cluster_manager_->httpConnPoolForCluster(
        "client", Envoy::Upstream::ResourcePriority::Default, proto, nullptr);
  }

  Envoy::Upstream::ClusterInfoConstSharedPtr cluster() {
    auto* cluster = cluster_manager_->get("client");
    return cluster == nullptr ? nullptr : cluster->info();
  }

  void prefetchPoolConnections() override;

private:
  Envoy::Api::Api& api_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::Stats::Store& store_;
  Envoy::Stats::ScopePtr scope_;
  Envoy::Http::HeaderMapImpl request_headers_;
  // These are declared order dependent. Changing ordering may trigger on assert upon
  // destruction when tls has been involved during usage.
  StatisticPtr connect_statistic_;
  StatisticPtr response_statistic_;
  const bool use_h2_;
  const UriPtr uri_;
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
  uint32_t request_body_size_{0};
  Envoy::Upstream::ClusterManagerPtr& cluster_manager_;
};

} // namespace Client
} // namespace Nighthawk