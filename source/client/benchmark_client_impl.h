#pragma once

#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/http/conn_pool.h"
#include "envoy/network/address.h"
#include "envoy/runtime/runtime.h"
#include "envoy/server/transport_socket_config.h"
#include "envoy/ssl/context_manager.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/store.h"
#include "envoy/upstream/upstream.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/statistic.h"

#include "common/common/logger.h"
#include "common/http/header_map_impl.h"
#include "common/runtime/runtime_impl.h"

#include "nighthawk/source/client/stream_decoder.h"
#include "nighthawk/source/common/ssl.h"
#include "nighthawk/source/common/utility.h"

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
  COUNTER(http_xxx)

struct BenchmarkClientStats {
  ALL_BENCHMARK_CLIENT_STATS(GENERATE_COUNTER_STRUCT)
};

class BenchmarkClientHttpImpl : public BenchmarkClient,
                                public StreamDecoderCompletionCallback,
                                public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  BenchmarkClientHttpImpl(Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher,
                          Envoy::Stats::Store& store, StatisticPtr&& connect_statistic,
                          StatisticPtr&& response_statistic, const Uri& uri, bool use_h2);

  void setConnectionLimit(uint64_t connection_limit) { connection_limit_ = connection_limit; }
  void setConnectionTimeout(std::chrono::seconds timeout) { timeout_ = timeout; }
  void setMaxPendingRequests(uint64_t max_pending_requests) {
    max_pending_requests_ = max_pending_requests;
  }

  // BenchmarkClient
  void initialize(Envoy::Runtime::Loader& runtime) override;
  void terminate() override;
  StatisticPtrMap statistics() const override;
  bool measureLatencies() const override { return measure_latencies_; }
  void setMeasureLatencies(bool measure_latencies) override {
    measure_latencies_ = measure_latencies;
  }
  bool tryStartOne(std::function<void()> caller_completion_callback) override;
  Envoy::Stats::Store& store() const override { return store_; }

  // StreamDecoderCompletionCallback
  void onComplete(bool success, const Envoy::Http::HeaderMap& headers) override;
  void onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason) override;

private:
  Envoy::Api::Api& api_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::Stats::Store& store_;
  Envoy::Stats::ScopePtr scope_;
  Envoy::Http::HeaderMapImpl request_headers_;
  // These are declared order dependent. Changing orderering may trigger on assert upon
  // destruction when tls has been involved during usage.
  std::unique_ptr<Envoy::Server::Configuration::TransportSocketFactoryContext>
      transport_socket_factory_context_;
  std::unique_ptr<Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl>
      ssl_context_manager_;
  Envoy::Upstream::ClusterInfoConstSharedPtr cluster_;
  StatisticPtr connect_statistic_;
  StatisticPtr response_statistic_;
  const bool use_h2_;
  const Uri uri_;
  std::chrono::seconds timeout_{5s};
  uint64_t connection_limit_{1};
  uint64_t max_pending_requests_{1};
  Envoy::Http::ConnectionPool::InstancePtr pool_;
  Envoy::Event::TimerPtr timer_;
  Envoy::Runtime::RandomGeneratorImpl generator_;
  uint64_t requests_completed_{};
  uint64_t requests_initiated_{};
  bool measure_latencies_{};
  BenchmarkClientStats benchmark_client_stats_;
};

} // namespace Client
} // namespace Nighthawk