#include <chrono>
#include <vector>

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
#include "test/mocks/common.h"
#include "test/mocks/runtime/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/server/utility.h"
#include "test/test_common/utility.h"
#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;
using Envoy::SaveArgAddress;

namespace Nighthawk {

class BenchmarkClientHttpTest : public Test {
public:
  BenchmarkClientHttpTest()
      : api_(thread_factory_, store_, time_system_, file_system_),
        dispatcher_(api_.allocateDispatcher()),
        cluster_manager_(std::make_unique<Envoy::Upstream::MockClusterManager>()),
        cluster_info_(std::make_unique<Envoy::Upstream::MockClusterInfo>()), response_code_("200") {
    EXPECT_CALL(cluster_manager(), httpConnPoolForCluster(_, _, _, _))
        .WillRepeatedly(Return(&pool_));
    EXPECT_CALL(cluster_manager(), get(_)).WillRepeatedly(Return(&thread_local_cluster_));
    EXPECT_CALL(thread_local_cluster_, info()).WillRepeatedly(Return(cluster_info_));
  }

  void testBasicFunctionality(absl::string_view uriPath, const uint64_t max_pending,
                              const uint64_t connection_limit, const bool use_h2,
                              const uint64_t amount_of_request) {
    if (client_ == nullptr) {
      setupBenchmarkClient(uriPath, use_h2, false);
      cluster_info().resetResourceManager(connection_limit, max_pending, 1024, 0, 1024);
    }

    EXPECT_CALL(stream_encoder_, encodeHeaders(_, _)).Times(AtLeast(1));

    EXPECT_CALL(pool_, newStream(_, _))
        .WillRepeatedly(Invoke([&](Envoy::Http::StreamDecoder& decoder,
                                   Envoy::Http::ConnectionPool::Callbacks& callbacks)
                                   -> Envoy::Http::ConnectionPool::Cancellable* {
          decoders_.push_back(&decoder);
          callbacks.onPoolReady(stream_encoder_, Envoy::Upstream::HostDescriptionConstSharedPtr{});
          return nullptr;
        }));

    client_->setMaxPendingRequests(max_pending);
    client_->setConnectionLimit(connection_limit);

    EXPECT_CALL(cluster_info(), resourceManager(_))
        .WillRepeatedly(
            ReturnRef(cluster_info_->resourceManager(Envoy::Upstream::ResourcePriority::Default)));

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
    dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
    EXPECT_EQ(max_pending, inflight_response_count);

    for (Envoy::Http::StreamDecoder* decoder : decoders_) {
      Envoy::Http::HeaderMapPtr response_headers{
          new Envoy::Http::TestHeaderMapImpl{{":status", response_code_}}};
      decoder->decodeHeaders(std::move(response_headers), false);
      Envoy::Buffer::OwnedImpl buffer(std::string('a', 97));
      decoder->decodeData(buffer, true);
    }
    decoders_.clear();
    dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
    EXPECT_EQ(0, inflight_response_count);
  }

  void setupBenchmarkClient(absl::string_view uriPath, bool use_https, bool use_h2) {
    const std::string address =
        Envoy::Network::Test::getLoopbackAddressUrlString(Envoy::Network::Address::IpVersion::v4);
    auto uri = std::make_unique<UriImpl>(
        fmt::format("{}://{}:{}{}", use_https ? "https" : "http", address, 80, uriPath));
    uri->resolve(*dispatcher_, Envoy::Network::DnsLookupFamily::V4Only);
    client_ = std::make_unique<Client::BenchmarkClientHttpImpl>(
        api_, *dispatcher_, store_, std::make_unique<StreamingStatistic>(),
        std::make_unique<StreamingStatistic>(), std::move(uri), use_h2, false, cluster_manager_);
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

  Envoy::Upstream::MockClusterManager& cluster_manager() {
    return dynamic_cast<Envoy::Upstream::MockClusterManager&>(*cluster_manager_);
  }
  Envoy::Upstream::MockClusterInfo& cluster_info() {
    return const_cast<Envoy::Upstream::MockClusterInfo&>(
        dynamic_cast<const Envoy::Upstream::MockClusterInfo&>(*cluster_info_));
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
  Envoy::Http::ConnectionPool::MockInstance pool_;
  Envoy::ProcessWide process_wide;
  std::vector<Envoy::Http::StreamDecoder*> decoders_;
  Envoy::Http::MockStreamEncoder stream_encoder_;
  Envoy::Upstream::MockThreadLocalCluster thread_local_cluster_;
  Envoy::Upstream::ClusterInfoConstSharedPtr cluster_info_;
  std::string response_code_;
};

TEST_F(BenchmarkClientHttpTest, BasicTestH1404) {
  response_code_ = "404";
  testBasicFunctionality("/lorem-ipsum-status-404", 1, 1, false, 10);
  EXPECT_EQ(1, getCounter("benchmark.http_4xx"));
  EXPECT_EQ(1, nonZeroValuedCounterCount());
}

// XXX(oschaaf): headermap rejects 601.
TEST_F(BenchmarkClientHttpTest, WeirdStatus) {
  response_code_ = "601";
  testBasicFunctionality("/601", 1, 1, false, 10);
  EXPECT_EQ(1, getCounter("benchmark.http_xxx"));
  EXPECT_EQ(1, nonZeroValuedCounterCount());
}

TEST_F(BenchmarkClientHttpTest, H1ConnectionFailure) {}

TEST_F(BenchmarkClientHttpTest, H1MultiConnectionFailure) {}

TEST_F(BenchmarkClientHttpTest, EnableLatencyMeasurement) {
  setupBenchmarkClient("/", false, false);
  EXPECT_EQ(false, client_->measureLatencies());
  testBasicFunctionality("/", 10, 1, false, 10);
  EXPECT_EQ(0, client_->statistics()["benchmark_http_client.queue_to_connect"]->count());
  EXPECT_EQ(0, client_->statistics()["benchmark_http_client.request_to_response"]->count());
  client_->setMeasureLatencies(true);
  testBasicFunctionality("/", 10, 1, false, 10);
  EXPECT_EQ(10, client_->statistics()["benchmark_http_client.queue_to_connect"]->count());
  EXPECT_EQ(10, client_->statistics()["benchmark_http_client.request_to_response"]->count());
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

TEST_F(BenchmarkClientHttpTest, PoolFailures) {
  setupBenchmarkClient("/lorem-ipsum-status-200", true, true);
  client_->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::ConnectionFailure);
  client_->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::Overflow);
  EXPECT_EQ(1, getCounter("benchmark.pool_overflow"));
  EXPECT_EQ(1, getCounter("benchmark.pool_connection_failure"));
}

TEST_F(BenchmarkClientHttpTest, RequestMethodPost) {
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

  EXPECT_CALL(stream_encoder_, encodeData(_, _)).Times(1);

  testBasicFunctionality("/", 1, 1, false, 1);

  EXPECT_EQ(1, getCounter("benchmark.http_2xx"));
  EXPECT_EQ(1, nonZeroValuedCounterCount());
}

// TODO(oschaaf): test protocol violations, stream resets, etc.

} // namespace Nighthawk
