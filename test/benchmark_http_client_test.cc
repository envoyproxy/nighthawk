#include <chrono>

#include "envoy/thread_local/thread_local.h"

#include "common/api/api_impl.h"
#include "common/common/compiler_requirements.h"
#include "common/common/thread_impl.h"
#include "common/event/dispatcher_impl.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/http/header_map_impl.h"
#include "common/network/dns_impl.h"
#include "common/network/utility.h"
#include "common/platform_util_impl.h"
#include "common/rate_limiter_impl.h"
#include "common/runtime/runtime_impl.h"
#include "common/sequencer_impl.h"
#include "common/statistic_impl.h"
#include "common/stats/isolated_store_impl.h"
#include "common/thread_local/thread_local_impl.h"
#include "common/uri_impl.h"
#include "common/utility.h"

#include "client/benchmark_client_impl.h"

#include "ares.h"
#include "exe/process_wide.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"
#include "test/mocks/runtime/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/server/utility.h"
#include "test/test_common/utility.h"
#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {

class BenchmarkClientTestBase : public Test {
public:
  BenchmarkClientTestBase()
      : api_(thread_factory_, store_, time_system_, file_system_),
        dispatcher_(api_.allocateDispatcher()),
        cluster_manager_(std::make_unique<NiceMock<Envoy::Upstream::MockClusterManager>>()) {}

  void testBasicFunctionality(absl::string_view uriPath, const uint64_t max_pending,
                              const uint64_t connection_limit, const bool use_h2,
                              const uint64_t amount_of_request) {
    setupBenchmarkClient(uriPath, use_h2, false);

    client_->setMaxPendingRequests(max_pending);
    client_->setConnectionLimit(connection_limit);

    const uint64_t amount = amount_of_request;
    uint64_t inflight_response_count = 0;

    std::function<void()> f = [this, &inflight_response_count]() {
      --inflight_response_count;
      if (inflight_response_count == 0) {
        dispatcher_->exit();
      }
    };

    for (uint64_t i = 0; i < amount; i++) {
      if (client_->tryStartOne(f)) {
        inflight_response_count++;
      }
    }

    EXPECT_EQ(max_pending, inflight_response_count);
    dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
  }

  virtual void setupBenchmarkClient(absl::string_view uriPath, bool use_h2,
                                    bool prefetch_connections) = 0;

  void doSetupBenchmarkClient(absl::string_view uriPath, bool use_https, bool use_h2,
                              bool prefetch_connections) {
    const std::string address =
        Envoy::Network::Test::getLoopbackAddressUrlString(Envoy::Network::Address::IpVersion::v4);
    auto uri = std::make_unique<UriImpl>(
        fmt::format("{}://{}:{}{}", use_https ? "https" : "http", address, 80, uriPath));
    uri->resolve(*dispatcher_, Envoy::Network::DnsLookupFamily::V4Only);
    client_ = std::make_unique<Client::BenchmarkClientHttpImpl>(
        api_, *dispatcher_, store_, std::make_unique<StreamingStatistic>(),
        std::make_unique<StreamingStatistic>(), std::move(uri), use_h2, prefetch_connections,
        cluster_manager_);
  }

  uint64_t nonZeroValuedCounterCount() {
    return Utility()
        .mapCountersFromStore(client_->store(),
                              [](absl::string_view, uint64_t value) { return value > 0; })
        .size();
  }

  uint64_t getCounter(absl::string_view name) {
    return client_->store().counter("client." + std::string(name)).value();
  }

  Envoy::Event::TestRealTimeSystem time_system_;
  Envoy::Thread::ThreadFactoryImplPosix thread_factory_;
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Api::Impl api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  Envoy::Runtime::RandomGeneratorImpl generator_;
  NiceMock<Envoy::ThreadLocal::MockInstance> tls_;
  NiceMock<Envoy::Runtime::MockLoader> runtime_;
  std::unique_ptr<Client::BenchmarkClientHttpImpl> client_;
  Envoy::Filesystem::InstanceImplPosix file_system_;
  Envoy::Upstream::ClusterManagerPtr cluster_manager_;
  Envoy::ProcessWide process_wide;
};

class BenchmarkClientHttpTest : public BenchmarkClientTestBase {
public:
  void setupBenchmarkClient(absl::string_view uriPath, bool use_h2,
                            bool prefetch_connections) override {
    doSetupBenchmarkClient(uriPath, false, use_h2, prefetch_connections);
  };
};

TEST_F(BenchmarkClientHttpTest, DISABLED_BasicTestH1404) {
  testBasicFunctionality("/lorem-ipsum-status-404", 1, 1, false, 10);

  EXPECT_EQ(1, getCounter("upstream_cx_http1_total"));
  EXPECT_EQ(0, getCounter("upstream_cx_protocol_error"));
  EXPECT_LE(97, getCounter("upstream_cx_rx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_LE(78, getCounter("upstream_cx_tx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_total"));
  EXPECT_EQ(1, getCounter("benchmark.http_4xx"));
  EXPECT_EQ(7, nonZeroValuedCounterCount());
}

// TODO(oschaaf): can't configure envoy to emit a weird status, fix
// this later.
TEST_F(BenchmarkClientHttpTest, DISABLED_WeirdStatus) {
  testBasicFunctionality("/601", 1, 1, false, 10);

  EXPECT_EQ(1, getCounter("upstream_cx_http1_total"));
  EXPECT_LE(3621, getCounter("upstream_cx_rx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_LE(78, getCounter("upstream_cx_tx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_total"));
  EXPECT_EQ(1, getCounter("benchmark.http_xxx"));
  EXPECT_EQ(7, nonZeroValuedCounterCount());
}

TEST_F(BenchmarkClientHttpTest, DISABLED_H1ConnectionFailure) {
  testBasicFunctionality("/lorem-ipsum-status-200", 1, 1, false, 10);

  EXPECT_EQ(1, getCounter("upstream_cx_connect_fail"));
  EXPECT_LE(1, getCounter("upstream_cx_http1_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_LE(1, getCounter("upstream_rq_pending_failure_eject"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_destroy_remote"));
  EXPECT_EQ(1, getCounter("upstream_cx_destroy"));
  EXPECT_EQ(8, nonZeroValuedCounterCount());
}

TEST_F(BenchmarkClientHttpTest, DISABLED_H1MultiConnectionFailure) {
  // Kill the test server, so we can't connect.
  // We allow ten connections and ten pending requests. We expect ten connection failures.
  testBasicFunctionality("/lorem-ipsum-status-200", 10, 10, false, 10);

  EXPECT_EQ(10, getCounter("upstream_cx_connect_fail"));
  EXPECT_LE(10, getCounter("upstream_cx_http1_total"));
  EXPECT_EQ(10, getCounter("upstream_cx_total"));
  EXPECT_LE(10, getCounter("upstream_rq_pending_failure_eject"));
  EXPECT_EQ(10, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(10, getCounter("upstream_cx_destroy_remote"));
  EXPECT_EQ(10, getCounter("upstream_cx_destroy"));
  EXPECT_EQ(8, nonZeroValuedCounterCount());
}

TEST_F(BenchmarkClientHttpTest, DISABLED_EnableLatencyMeasurement) {
  setupBenchmarkClient("/", false, false);
  int callback_count = 0;

  EXPECT_EQ(false, client_->measureLatencies());
  EXPECT_EQ(true, client_->tryStartOne([&]() {
    callback_count++;
    dispatcher_->exit();
  }));
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(0, client_->statistics()["benchmark_http_client.queue_to_connect"]->count());
  EXPECT_EQ(0, client_->statistics()["benchmark_http_client.request_to_response"]->count());
  client_->setMeasureLatencies(true);
  EXPECT_EQ(true, client_->measureLatencies());
  EXPECT_EQ(true, client_->tryStartOne([&]() {
    callback_count++;
    dispatcher_->exit();
  }));

  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);

  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(1, client_->statistics()["benchmark_http_client.queue_to_connect"]->count());
  EXPECT_EQ(1, client_->statistics()["benchmark_http_client.request_to_response"]->count());
}

TEST_F(BenchmarkClientHttpTest, StatusTrackingInOnComplete) {
  auto uri = std::make_unique<UriImpl>("http://foo/");
  auto store = std::make_unique<Envoy::Stats::IsolatedStoreImpl>();
  client_ = std::make_unique<Client::BenchmarkClientHttpImpl>(
      api_, *dispatcher_, *store, std::make_unique<StreamingStatistic>(),
      std::make_unique<StreamingStatistic>(), std::move(uri), false, false, cluster_manager_);
  Envoy::Http::HeaderMapImpl header;

  auto& status = header.insertStatus();

  status.value(1);
  client_->onComplete(true, header);
  status.value(100);
  client_->onComplete(true, header);
  status.value(200);
  client_->onComplete(true, header);
  status.value(300);
  client_->onComplete(true, header);
  status.value(400);
  client_->onComplete(true, header);
  status.value(500);
  client_->onComplete(true, header);
  status.value(600);
  client_->onComplete(true, header);
  status.value(200);
  // Shouldn't be counted by status, should add to stream reset.
  client_->onComplete(false, header);

  EXPECT_EQ(1, getCounter("benchmark.http_2xx"));
  EXPECT_EQ(1, getCounter("benchmark.http_3xx"));
  EXPECT_EQ(1, getCounter("benchmark.http_4xx"));
  EXPECT_EQ(1, getCounter("benchmark.http_5xx"));
  EXPECT_EQ(2, getCounter("benchmark.http_xxx"));
  EXPECT_EQ(1, getCounter("benchmark.stream_resets"));

  client_.reset();
}

TEST_F(BenchmarkClientHttpTest, DISABLED_ConnectionPrefetching) {
  setupBenchmarkClient("/", false, true);
  client_->setConnectionLimit(50);
  EXPECT_EQ(true, client_->tryStartOne([&]() { dispatcher_->exit(); }));
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
  EXPECT_EQ(50, getCounter("upstream_cx_total"));
}

// TODO(oschaaf): Turns out this feature is h/2 only as of writing this.
// Consider logging a warning/error when attempting to configure this with
// H1 enabled.
TEST_F(BenchmarkClientHttpTest, DISABLED_CapRequestConcurrency) {
  setupBenchmarkClient("/lorem-ipsum-status-200", true, false);
  const uint64_t requests = 4;
  uint64_t inflight_response_count = requests;

  // We configure so that max requests is the only thing that can be throttling.
  client_->setMaxPendingRequests(requests);
  client_->setConnectionLimit(requests);
  client_->setMaxActiveRequests(1);

  std::function<void()> f = [this, &inflight_response_count]() {
    --inflight_response_count;
    if (inflight_response_count == 3) {
      dispatcher_->exit();
    }
  };
  for (uint64_t i = 0; i < requests; i++) {
    EXPECT_EQ(true, client_->tryStartOne(f));
  }
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
  EXPECT_EQ(1, getCounter("benchmark.http_2xx"));
  EXPECT_EQ(1, getCounter("upstream_rq_total"));
  EXPECT_EQ(3, getCounter("upstream_rq_pending_overflow"));
  EXPECT_EQ(3, getCounter("benchmark.pool_overflow"));
}

TEST_F(BenchmarkClientHttpTest, DISABLED_MaxRequestsPerConnection) {
  setupBenchmarkClient("/lorem-ipsum-status-200", false, false);
  const uint64_t requests = 4;
  uint64_t inflight_response_count = requests;

  // We configure so that max requests is the only thing that can be throttling.
  client_->setMaxPendingRequests(requests);
  client_->setConnectionLimit(requests);
  client_->setMaxActiveRequests(1024);
  client_->setMaxRequestsPerConnection(1);

  std::function<void()> f = [this, &inflight_response_count]() {
    --inflight_response_count;
    if (inflight_response_count == 0) {
      dispatcher_->exit();
    }
  };
  for (uint64_t i = 0; i < requests; i++) {
    EXPECT_EQ(true, client_->tryStartOne(f));
  }
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);

  EXPECT_EQ(requests, getCounter("benchmark.http_2xx"));
  EXPECT_EQ(requests, getCounter("upstream_cx_http1_total"));
}

TEST_F(BenchmarkClientHttpTest, DISABLED_RequestMethodPost) {
  setupBenchmarkClient("/", false, true);
  EXPECT_EQ("GET", client_->requestHeaders().Method()->value().getStringView());
  client_->setRequestMethod(envoy::api::v2::core::RequestMethod::POST);
  client_->setRequestHeader("a", "b");
  client_->setRequestHeader("c", "d");
  client_->setRequestBodySize(1313);

  EXPECT_EQ("POST", client_->requestHeaders().Method()->value().getStringView());
  EXPECT_EQ(
      "b",
      client_->requestHeaders().get(Envoy::Http::LowerCaseString("a"))->value().getStringView());
  EXPECT_EQ(
      "d",
      client_->requestHeaders().get(Envoy::Http::LowerCaseString("c"))->value().getStringView());

  EXPECT_EQ(true, client_->tryStartOne([&]() { dispatcher_->exit(); }));
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);

  EXPECT_EQ(1, getCounter("benchmark.http_4xx"));
  EXPECT_EQ(1407, getCounter("upstream_cx_tx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_overflow"));
  EXPECT_EQ(116, getCounter("upstream_cx_rx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_close_notify"));
  EXPECT_EQ(1, getCounter("upstream_cx_http1_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_close_notify"));
  EXPECT_EQ(1, getCounter("upstream_cx_destroy_local"));
  EXPECT_EQ(11, nonZeroValuedCounterCount());
}

// TODO(oschaaf): test protocol violations, stream resets, etc.

} // namespace Nighthawk
