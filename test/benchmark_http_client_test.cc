#include <vector>

#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/exe/process_wide.h"
#include "external/envoy/test/mocks/common.h"
#include "external/envoy/test/mocks/runtime/mocks.h"
#include "external/envoy/test/mocks/stats/mocks.h"
#include "external/envoy/test/mocks/stream_info/mocks.h"
#include "external/envoy/test/mocks/thread_local/mocks.h"
#include "external/envoy/test/mocks/upstream/mocks.h"
#include "external/envoy/test/test_common/network_utility.h"

#include "common/request_impl.h"
#include "common/statistic_impl.h"
#include "common/uri_impl.h"
#include "common/utility.h"

#include "client/benchmark_client_impl.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {

class BenchmarkClientHttpTest : public Test {
public:
  BenchmarkClientHttpTest()
      : api_(Envoy::Api::createApiForTest(time_system_)),
        dispatcher_(api_->allocateDispatcher("test_thread")),
        cluster_manager_(std::make_unique<Envoy::Upstream::MockClusterManager>()),
        cluster_info_(std::make_unique<Envoy::Upstream::MockClusterInfo>()),
        http_tracer_(std::make_unique<Envoy::Tracing::MockHttpTracer>()), response_code_("200") {
    EXPECT_CALL(cluster_manager(), httpConnPoolForCluster(_, _, _, _))
        .WillRepeatedly(Return(&pool_));
    EXPECT_CALL(cluster_manager(), get(_)).WillRepeatedly(Return(&thread_local_cluster_));
    EXPECT_CALL(thread_local_cluster_, info()).WillRepeatedly(Return(cluster_info_));

    auto& tracer = static_cast<Envoy::Tracing::MockHttpTracer&>(*http_tracer_);
    EXPECT_CALL(tracer, startSpan_(_, _, _, _))
        .WillRepeatedly(
            Invoke([&](const Envoy::Tracing::Config& config, const Envoy::Http::HeaderMap&,
                       const Envoy::StreamInfo::StreamInfo&,
                       const Envoy::Tracing::Decision) -> Envoy::Tracing::Span* {
              EXPECT_EQ(Envoy::Tracing::OperationName::Egress, config.operationName());
              auto* span = new NiceMock<Envoy::Tracing::MockSpan>();
              return span;
            }));
    request_generator_ = []() {
      auto header = std::make_shared<Envoy::Http::TestRequestHeaderMapImpl>(
          std::initializer_list<std::pair<std::string, std::string>>(
              {{":scheme", "http"}, {":method", "GET"}, {":path", "/"}, {":host", "localhost"}}));
      return std::make_unique<RequestImpl>(header);
    };
  }

  void testBasicFunctionality(const uint64_t max_pending, const uint64_t connection_limit,
                              const uint64_t amount_of_request) {
    if (client_ == nullptr) {
      setupBenchmarkClient();
      cluster_info().resetResourceManager(connection_limit, max_pending, 1024, 0, 1024);
    }

    EXPECT_CALL(stream_encoder_, encodeHeaders(_, _)).Times(AtLeast(1));

    EXPECT_CALL(pool_, newStream(_, _))
        .WillRepeatedly(Invoke([&](Envoy::Http::ResponseDecoder& decoder,
                                   Envoy::Http::ConnectionPool::Callbacks& callbacks)
                                   -> Envoy::Http::ConnectionPool::Cancellable* {
          decoders_.push_back(&decoder);
          NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info;
          callbacks.onPoolReady(stream_encoder_, Envoy::Upstream::HostDescriptionConstSharedPtr{},
                                stream_info);
          return nullptr;
        }));

    client_->setMaxPendingRequests(max_pending);
    client_->setConnectionLimit(connection_limit);

    EXPECT_CALL(cluster_info(), resourceManager(_))
        .WillRepeatedly(
            ReturnRef(cluster_info_->resourceManager(Envoy::Upstream::ResourcePriority::Default)));

    const uint64_t amount = amount_of_request;
    uint64_t inflight_response_count = 0;

    Client::CompletionCallback f = [this, &inflight_response_count](bool, bool) {
      --inflight_response_count;
      if (inflight_response_count == 0) {
        dispatcher_->exit();
      }
    };

    for (uint64_t i = 0; i < amount; i++) {
      if (client_->tryStartRequest(f)) {
        inflight_response_count++;
      }
    }

    const uint64_t max_in_flight_allowed = max_pending + connection_limit;
    // If amount_of_request >= max_in_flight_allowed, we are not able to add more request.
    if (amount >= max_in_flight_allowed) {
      EXPECT_FALSE(client_->tryStartRequest(f));
    }

    dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
    // Expect inflight_response_count to be equal to min(amount, max_in_flight_allowed).
    EXPECT_EQ(amount < max_in_flight_allowed ? amount : max_in_flight_allowed,
              inflight_response_count);

    for (Envoy::Http::ResponseDecoder* decoder : decoders_) {
      Envoy::Http::ResponseHeaderMapPtr response_headers{
          new Envoy::Http::TestResponseHeaderMapImpl{{":status", response_code_}}};
      decoder->decodeHeaders(std::move(response_headers), false);
      Envoy::Buffer::OwnedImpl buffer(std::string(97, 'a'));
      decoder->decodeData(buffer, true);
    }
    decoders_.clear();
    dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
    EXPECT_EQ(0, inflight_response_count);
  }

  void setupBenchmarkClient() {
    client_ = std::make_unique<Client::BenchmarkClientHttpImpl>(
        *api_, *dispatcher_, mock_store_, std::make_unique<StreamingStatistic>(),
        std::make_unique<StreamingStatistic>(), std::make_unique<StreamingStatistic>(),
        std::make_unique<StreamingStatistic>(), false, cluster_manager_, http_tracer_, "benchmark",
        request_generator_, true);
  }

  uint64_t getCounter(absl::string_view name) {
    return client_->scope().counterFromString(std::string(name)).value();
  }

  Envoy::Upstream::MockClusterManager& cluster_manager() {
    return dynamic_cast<Envoy::Upstream::MockClusterManager&>(*cluster_manager_);
  }
  Envoy::Upstream::MockClusterInfo& cluster_info() {
    return const_cast<Envoy::Upstream::MockClusterInfo&>(
        dynamic_cast<const Envoy::Upstream::MockClusterInfo&>(*cluster_info_));
  }

  Envoy::Event::TestRealTimeSystem time_system_;
  // deliverHistogramToSinks() is not implemented in IsolatedStoreImpl so test with a mock store.
  Envoy::Stats::MockIsolatedStatsStore mock_store_;
  Envoy::Api::ApiPtr api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  Envoy::Runtime::RandomGeneratorImpl generator_;
  NiceMock<Envoy::ThreadLocal::MockInstance> tls_;
  NiceMock<Envoy::Runtime::MockLoader> runtime_;
  std::unique_ptr<Client::BenchmarkClientHttpImpl> client_;
  Envoy::Upstream::ClusterManagerPtr cluster_manager_;
  Envoy::Http::ConnectionPool::MockInstance pool_;
  Envoy::ProcessWide process_wide;
  std::vector<Envoy::Http::ResponseDecoder*> decoders_;
  NiceMock<Envoy::Http::MockRequestEncoder> stream_encoder_;
  Envoy::Upstream::MockThreadLocalCluster thread_local_cluster_;
  Envoy::Upstream::ClusterInfoConstSharedPtr cluster_info_;
  Envoy::Tracing::HttpTracerSharedPtr http_tracer_;
  std::string response_code_;
  RequestGenerator request_generator_;
};

TEST_F(BenchmarkClientHttpTest, BasicTestH1200) {
  response_code_ = "200";
  testBasicFunctionality(2, 3, 10);
  EXPECT_EQ(5, getCounter("http_2xx"));
  EXPECT_EQ(5, getCounter("total_req_sent"));
}

TEST_F(BenchmarkClientHttpTest, BasicTestH1300) {
  response_code_ = "300";
  testBasicFunctionality(0, 11, 10);
  EXPECT_EQ(10, getCounter("http_3xx"));
  EXPECT_EQ(10, getCounter("total_req_sent"));
}

TEST_F(BenchmarkClientHttpTest, BasicTestH1404) {
  response_code_ = "404";
  testBasicFunctionality(0, 1, 10);
  EXPECT_EQ(1, getCounter("http_4xx"));
  EXPECT_EQ(1, getCounter("total_req_sent"));
}

TEST_F(BenchmarkClientHttpTest, WeirdStatus) {
  response_code_ = "601";
  testBasicFunctionality(0, 1, 10);
  EXPECT_EQ(1, getCounter("http_xxx"));
  EXPECT_EQ(1, getCounter("total_req_sent"));
}

TEST_F(BenchmarkClientHttpTest, EnableLatencyMeasurement) {
  setupBenchmarkClient();
  EXPECT_EQ(false, client_->shouldMeasureLatencies());
  EXPECT_CALL(mock_store_, deliverHistogramToSinks(_, _)).Times(0);
  testBasicFunctionality(10, 1, 10);
  EXPECT_EQ(0, client_->statistics()["benchmark_http_client.queue_to_connect"]->count());
  EXPECT_EQ(0, client_->statistics()["benchmark_http_client.request_to_response"]->count());
  EXPECT_EQ(10, getCounter("total_req_sent"));
  client_->setShouldMeasureLatencies(true);
  EXPECT_CALL(mock_store_, deliverHistogramToSinks(_, _)).Times(10);
  testBasicFunctionality(10, 1, 10);
  EXPECT_EQ(10, client_->statistics()["benchmark_http_client.queue_to_connect"]->count());
  EXPECT_EQ(10, client_->statistics()["benchmark_http_client.request_to_response"]->count());
  EXPECT_EQ(20, getCounter("total_req_sent"));
}

TEST_F(BenchmarkClientHttpTest, ExportSuccessLatency) {
  setupBenchmarkClient();
  uint64_t latency = 10;
  EXPECT_CALL(mock_store_, deliverHistogramToSinks(Property(&Envoy::Stats::Metric::name,
                                                            "benchmark.latency_on_success_req"),
                                                   latency))
      .Times(1);
  EXPECT_CALL(mock_store_,
              deliverHistogramToSinks(
                  Property(&Envoy::Stats::Metric::name, "benchmark.latency_on_error_req"), latency))
      .Times(0);
  client_->exportLatency(/*response_code=*/200, /*latency_us=*/latency);
}

TEST_F(BenchmarkClientHttpTest, ExportErrorLatency) {
  setupBenchmarkClient();
  uint64_t latency = 10;
  EXPECT_CALL(mock_store_, deliverHistogramToSinks(Property(&Envoy::Stats::Metric::name,
                                                            "benchmark.latency_on_success_req"),
                                                   latency))
      .Times(0);
  EXPECT_CALL(mock_store_,
              deliverHistogramToSinks(
                  Property(&Envoy::Stats::Metric::name, "benchmark.latency_on_error_req"), latency))
      .Times(1);
  client_->exportLatency(/*response_code=*/500, /*latency_us=*/latency);
}

TEST_F(BenchmarkClientHttpTest, StatusTrackingInOnComplete) {
  setupBenchmarkClient();
  Envoy::Http::ResponseHeaderMapPtr header = Envoy::Http::ResponseHeaderMapImpl::create();

  header->setStatus(1);
  client_->onComplete(true, *header);
  header->setStatus(100);
  client_->onComplete(true, *header);
  header->setStatus(200);
  client_->onComplete(true, *header);
  header->setStatus(300);
  client_->onComplete(true, *header);
  header->setStatus(400);
  client_->onComplete(true, *header);
  header->setStatus(500);
  client_->onComplete(true, *header);
  header->setStatus(600);
  client_->onComplete(true, *header);
  header->setStatus(200);
  // Shouldn't be counted by status, should add to stream reset.
  client_->onComplete(false, *header);

  EXPECT_EQ(1, getCounter("http_2xx"));
  EXPECT_EQ(1, getCounter("http_3xx"));
  EXPECT_EQ(1, getCounter("http_4xx"));
  EXPECT_EQ(1, getCounter("http_5xx"));
  EXPECT_EQ(2, getCounter("http_xxx"));
  EXPECT_EQ(1, getCounter("stream_resets"));

  client_.reset();
}

TEST_F(BenchmarkClientHttpTest, PoolFailures) {
  setupBenchmarkClient();
  client_->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::LocalConnectionFailure);
  client_->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::RemoteConnectionFailure);
  client_->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::Overflow);
  client_->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::Timeout);
  EXPECT_EQ(1, getCounter("pool_overflow"));
  EXPECT_EQ(2, getCounter("pool_connection_failure"));
}

TEST_F(BenchmarkClientHttpTest, RequestMethodPost) {
  request_generator_ = []() {
    auto header = std::make_shared<Envoy::Http::TestRequestHeaderMapImpl>(
        std::initializer_list<std::pair<std::string, std::string>>({{":scheme", "http"},
                                                                    {":method", "POST"},
                                                                    {":path", "/"},
                                                                    {":host", "localhost"},
                                                                    {"a", "b"},
                                                                    {"c", "d"},
                                                                    {"Content-Length", "1313"}}));
    return std::make_unique<RequestImpl>(header);
  };

  EXPECT_CALL(stream_encoder_, encodeData(_, _)).Times(1);
  testBasicFunctionality(1, 1, 1);
  EXPECT_EQ(1, getCounter("http_2xx"));
}

TEST_F(BenchmarkClientHttpTest, BadContentLength) {
  request_generator_ = []() {
    auto header = std::make_shared<Envoy::Http::TestRequestHeaderMapImpl>(
        std::initializer_list<std::pair<std::string, std::string>>({{":scheme", "http"},
                                                                    {":method", "POST"},
                                                                    {":path", "/"},
                                                                    {":host", "localhost"},
                                                                    {"Content-Length", "-1313"}}));
    return std::make_unique<RequestImpl>(header);
  };

  EXPECT_CALL(stream_encoder_, encodeData(_, _)).Times(0);
  testBasicFunctionality(1, 1, 1);
  EXPECT_EQ(1, getCounter("http_2xx"));
}

} // namespace Nighthawk
