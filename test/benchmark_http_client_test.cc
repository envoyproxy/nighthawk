#include <vector>

#include "external/envoy/source/common/common/random_generator.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/source/exe/process_wide.h"
#include "external/envoy/test/mocks/common.h"
#include "external/envoy/test/mocks/runtime/mocks.h"
#include "external/envoy/test/mocks/stream_info/mocks.h"
#include "external/envoy/test/mocks/thread_local/mocks.h"
#include "external/envoy/test/mocks/upstream/mocks.h"
#include "external/envoy/test/test_common/network_utility.h"

#include "source/client/benchmark_client_impl.h"
#include "source/common/request_impl.h"
#include "source/common/statistic_impl.h"
#include "source/common/uri_impl.h"
#include "source/common/utility.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {

namespace {
// Helper function to get headers in a set that should be verified during the test.
std::string getPathFromRequest(const Envoy::Http::RequestHeaderMap& header) {
  return std::string(header.getPathValue());
}

// This struct contains necessary information for setting up benchmark client to get requests in
// verifyBenchmarkClientProcessesExpectedInflightRequests.
struct ClientSetupParameters {
  // max_pending corresponds to the number of max_pending requests. connection limit corresponds to
  // the number of maximum connections allowed. amount refers to the number of requests expected.
  // The generator is a function that returns requests.
  ClientSetupParameters(const uint64_t max_pending, const uint64_t connection_limit,
                        const uint64_t amount, const RequestGenerator& generator)
      : max_pending_requests(max_pending), max_connection_limit(connection_limit),
        amount_of_requests(amount), request_generator(generator) {}
  const uint64_t max_pending_requests;
  const uint64_t max_connection_limit;
  const uint64_t amount_of_requests;
  const RequestGenerator& request_generator;
};
} // namespace

class BenchmarkClientHttpTest : public Test {
public:
  BenchmarkClientHttpTest()
      : api_(Envoy::Api::createApiForTest(time_system_)),
        dispatcher_(api_->allocateDispatcher("test_thread")),
        cluster_manager_(std::make_unique<Envoy::Upstream::MockClusterManager>()),
        cluster_info_(std::make_unique<Envoy::Upstream::MockClusterInfo>()),
        http_tracer_(std::make_unique<Envoy::Tracing::MockHttpTracer>()), response_code_("200"),
        statistic_(std::make_unique<StreamingStatistic>(), std::make_unique<StreamingStatistic>(),
                   std::make_unique<StreamingStatistic>(), std::make_unique<StreamingStatistic>(),
                   std::make_unique<StreamingStatistic>(), std::make_unique<StreamingStatistic>(),
                   std::make_unique<StreamingStatistic>(), std::make_unique<StreamingStatistic>(),
                   std::make_unique<StreamingStatistic>(), std::make_unique<StreamingStatistic>(),
                   std::make_unique<StreamingStatistic>()) {
    auto header_map_param = std::initializer_list<std::pair<std::string, std::string>>{
        {":scheme", "http"}, {":method", "GET"}, {":path", "/"}, {":host", "localhost"}};
    default_header_map_ =
        (std::make_shared<Envoy::Http::TestRequestHeaderMapImpl>(header_map_param));
    EXPECT_CALL(cluster_manager(), getThreadLocalCluster(_))
        .WillRepeatedly(Return(&thread_local_cluster_));
    EXPECT_CALL(thread_local_cluster_, info()).WillRepeatedly(Return(cluster_info_));
    EXPECT_CALL(thread_local_cluster_, httpConnPool(_, _, _))
        .WillRepeatedly(Return(Envoy::Upstream::HttpPoolData([]() {}, &pool_)));

    auto& tracer = static_cast<Envoy::Tracing::MockHttpTracer&>(*http_tracer_);
    EXPECT_CALL(tracer, startSpan_(_, _, _, _))
        .WillRepeatedly([](const Envoy::Tracing::Config& config, const Envoy::Http::HeaderMap&,
                           const Envoy::StreamInfo::StreamInfo&,
                           const Envoy::Tracing::Decision) -> Envoy::Tracing::Span* {
          EXPECT_EQ(Envoy::Tracing::OperationName::Egress, config.operationName());
          auto* span = new NiceMock<Envoy::Tracing::MockSpan>();
          return span;
        });
  }
  // Default function for request generator when the content doesn't matter.
  RequestGenerator getDefaultRequestGenerator() {
    RequestGenerator request_generator = [this]() {
      auto returned_request_impl = std::make_unique<RequestImpl>(default_header_map_);
      return returned_request_impl;
    };
    return request_generator;
  }
  // Primary testing method. Confirms that connection limits are met and number of requests are
  // correct. If header expectations is not null, also checks the header expectations, if null, it
  // is ignored.
  void verifyBenchmarkClientProcessesExpectedInflightRequests(
      ClientSetupParameters& client_setup_parameters,
      const absl::flat_hash_set<std::string>* header_expectations = nullptr) {
    if (client_ == nullptr) {
      setupBenchmarkClient(client_setup_parameters.request_generator);
      cluster_info().resetResourceManager(client_setup_parameters.max_connection_limit,
                                          client_setup_parameters.max_pending_requests, 1024, 0,
                                          1024);
    }
    // This is where we store the properties of headers that are passed to the stream encoder. We
    // verify later that these match expected headers.
    absl::flat_hash_set<std::string> called_headers;
    EXPECT_CALL(stream_encoder_, encodeHeaders(_, _)).Times(AtLeast(1));
    ON_CALL(stream_encoder_, encodeHeaders(_, _))
        .WillByDefault(
            WithArgs<0>(([&called_headers](const Envoy::Http::RequestHeaderMap& specific_request) {
              called_headers.insert(getPathFromRequest(specific_request));
              return Envoy::Http::Status();
            })));

    EXPECT_CALL(pool_, newStream(_, _))
        .WillRepeatedly([this](Envoy::Http::ResponseDecoder& decoder,
                               Envoy::Http::ConnectionPool::Callbacks& callbacks)
                            -> Envoy::Http::ConnectionPool::Cancellable* {
          decoders_.push_back(&decoder);
          NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info;
          callbacks.onPoolReady(stream_encoder_, Envoy::Upstream::HostDescriptionConstSharedPtr{},
                                stream_info, {} /*absl::optional<Envoy::Http::Protocol> protocol*/);
          return nullptr;
        });

    client_->setMaxPendingRequests(client_setup_parameters.max_pending_requests);
    client_->setConnectionLimit(client_setup_parameters.max_connection_limit);

    EXPECT_CALL(cluster_info(), resourceManager(_))
        .WillRepeatedly(
            ReturnRef(cluster_info_->resourceManager(Envoy::Upstream::ResourcePriority::Default)));

    const uint64_t amount = client_setup_parameters.amount_of_requests;
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

    const uint64_t max_in_flight_allowed =
        client_setup_parameters.max_pending_requests + client_setup_parameters.max_connection_limit;
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
    // If we have no expectations, then we don't test.
    if (header_expectations != nullptr) {
      EXPECT_THAT((*header_expectations), UnorderedElementsAreArray(called_headers));
    }
  }

  // Used to set up benchmarkclient. Especially from within
  // verifyBenchmarkClientProcessesExpectedInflightRequests.
  void setupBenchmarkClient(const RequestGenerator& request_generator) {
    client_ = std::make_unique<Client::BenchmarkClientHttpImpl>(
        *api_, *dispatcher_, store_, statistic_, /*use_h2*/ false, cluster_manager_, http_tracer_,
        "benchmark", request_generator, /*provide_resource_backpressure*/ true,
        /*response_header_with_latency_input=*/"");
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
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Api::ApiPtr api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  Envoy::Random::RandomGeneratorImpl generator_;
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
  int worker_number_{0};
  Client::BenchmarkClientStatistic statistic_;
  std::shared_ptr<Envoy::Http::RequestHeaderMap> default_header_map_;
};

TEST_F(BenchmarkClientHttpTest, BasicTestH1200) {
  response_code_ = "200";
  RequestGenerator default_request_generator = getDefaultRequestGenerator();
  auto client_setup_param = ClientSetupParameters(2, 3, 10, default_request_generator);
  verifyBenchmarkClientProcessesExpectedInflightRequests(client_setup_param);
  EXPECT_EQ(5, getCounter("http_2xx"));
}

TEST_F(BenchmarkClientHttpTest, BasicTestH1300) {
  response_code_ = "300";
  RequestGenerator default_request_generator = getDefaultRequestGenerator();
  auto client_setup_param = ClientSetupParameters(0, 11, 10, default_request_generator);
  verifyBenchmarkClientProcessesExpectedInflightRequests(client_setup_param);
  EXPECT_EQ(10, getCounter("http_3xx"));
}

TEST_F(BenchmarkClientHttpTest, BasicTestH1404) {
  response_code_ = "404";
  RequestGenerator default_request_generator = getDefaultRequestGenerator();
  auto client_setup_param = ClientSetupParameters(0, 1, 10, default_request_generator);
  verifyBenchmarkClientProcessesExpectedInflightRequests(client_setup_param);
  EXPECT_EQ(1, getCounter("http_4xx"));
}

TEST_F(BenchmarkClientHttpTest, WeirdStatus) {
  response_code_ = "601";
  RequestGenerator default_request_generator = getDefaultRequestGenerator();
  auto client_setup_param = ClientSetupParameters(0, 1, 10, default_request_generator);
  verifyBenchmarkClientProcessesExpectedInflightRequests(client_setup_param);
  EXPECT_EQ(1, getCounter("http_xxx"));
}

TEST_F(BenchmarkClientHttpTest, EnableLatencyMeasurement) {
  setupBenchmarkClient(getDefaultRequestGenerator());
  EXPECT_EQ(false, client_->shouldMeasureLatencies());
  RequestGenerator default_request_generator = getDefaultRequestGenerator();
  auto client_setup_param = ClientSetupParameters(10, 1, 10, default_request_generator);
  verifyBenchmarkClientProcessesExpectedInflightRequests(client_setup_param);
  EXPECT_EQ(0, client_->statistics()["benchmark_http_client.queue_to_connect"]->count());
  EXPECT_EQ(0, client_->statistics()["benchmark_http_client.request_to_response"]->count());
  EXPECT_EQ(10, client_->statistics()["benchmark_http_client.response_header_size"]->count());
  EXPECT_EQ(10, client_->statistics()["benchmark_http_client.response_body_size"]->count());
  EXPECT_EQ(0, client_->statistics()["benchmark_http_client.latency_2xx"]->count());
  client_->setShouldMeasureLatencies(true);

  verifyBenchmarkClientProcessesExpectedInflightRequests(client_setup_param);
  EXPECT_EQ(10, client_->statistics()["benchmark_http_client.queue_to_connect"]->count());
  EXPECT_EQ(10, client_->statistics()["benchmark_http_client.request_to_response"]->count());
  EXPECT_EQ(20, client_->statistics()["benchmark_http_client.response_header_size"]->count());
  EXPECT_EQ(20, client_->statistics()["benchmark_http_client.response_body_size"]->count());
  EXPECT_EQ(10, client_->statistics()["benchmark_http_client.latency_2xx"]->count());
}

TEST_F(BenchmarkClientHttpTest, ExportSuccessLatency) {
  RequestGenerator default_request_generator = getDefaultRequestGenerator();
  setupBenchmarkClient(default_request_generator);
  uint64_t latency_ns = 10;
  client_->exportLatency(/*response_code=*/200, latency_ns);
  client_->exportLatency(/*response_code=*/200, latency_ns);
  EXPECT_EQ(2, client_->statistics()["benchmark_http_client.latency_2xx"]->count());
  EXPECT_DOUBLE_EQ(latency_ns, client_->statistics()["benchmark_http_client.latency_2xx"]->mean());
}

TEST_F(BenchmarkClientHttpTest, ExportErrorLatency) {
  RequestGenerator default_request_generator = getDefaultRequestGenerator();
  setupBenchmarkClient(default_request_generator);
  client_->exportLatency(/*response_code=*/100, /*latency_ns=*/1);
  client_->exportLatency(/*response_code=*/300, /*latency_ns=*/3);
  client_->exportLatency(/*response_code=*/400, /*latency_ns=*/4);
  client_->exportLatency(/*response_code=*/500, /*latency_ns=*/5);
  client_->exportLatency(/*response_code=*/600, /*latency_ns=*/6);
  EXPECT_EQ(1, client_->statistics()["benchmark_http_client.latency_1xx"]->count());
  EXPECT_DOUBLE_EQ(1, client_->statistics()["benchmark_http_client.latency_1xx"]->mean());
  EXPECT_EQ(1, client_->statistics()["benchmark_http_client.latency_xxx"]->count());
  EXPECT_DOUBLE_EQ(3, client_->statistics()["benchmark_http_client.latency_3xx"]->mean());
  EXPECT_EQ(1, client_->statistics()["benchmark_http_client.latency_xxx"]->count());
  EXPECT_DOUBLE_EQ(4, client_->statistics()["benchmark_http_client.latency_4xx"]->mean());
  EXPECT_EQ(1, client_->statistics()["benchmark_http_client.latency_xxx"]->count());
  EXPECT_DOUBLE_EQ(5, client_->statistics()["benchmark_http_client.latency_5xx"]->mean());
  EXPECT_EQ(1, client_->statistics()["benchmark_http_client.latency_xxx"]->count());
  EXPECT_DOUBLE_EQ(6, client_->statistics()["benchmark_http_client.latency_xxx"]->mean());
}

TEST_F(BenchmarkClientHttpTest, StatusTrackingInOnComplete) {
  RequestGenerator default_request_generator = getDefaultRequestGenerator();
  setupBenchmarkClient(default_request_generator);
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
  RequestGenerator default_request_generator = getDefaultRequestGenerator();
  setupBenchmarkClient(default_request_generator);
  client_->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::LocalConnectionFailure);
  client_->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::RemoteConnectionFailure);
  client_->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::Overflow);
  client_->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::Timeout);
  EXPECT_EQ(1, getCounter("pool_overflow"));
  EXPECT_EQ(2, getCounter("pool_connection_failure"));
}

TEST_F(BenchmarkClientHttpTest, RequestMethodPost) {
  RequestGenerator request_generator = []() {
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

  EXPECT_CALL(stream_encoder_, encodeData(_, _));
  auto client_setup_parameters = ClientSetupParameters(1, 1, 1, request_generator);
  verifyBenchmarkClientProcessesExpectedInflightRequests(client_setup_parameters);
  EXPECT_EQ(1, getCounter("http_2xx"));
}

TEST_F(BenchmarkClientHttpTest, BadContentLength) {
  RequestGenerator request_generator = []() {
    auto header = std::make_shared<Envoy::Http::TestRequestHeaderMapImpl>(
        std::initializer_list<std::pair<std::string, std::string>>({{":scheme", "http"},
                                                                    {":method", "POST"},
                                                                    {":path", "/"},
                                                                    {":host", "localhost"},
                                                                    {"Content-Length", "-1313"}}));
    return std::make_unique<RequestImpl>(header);
  };

  EXPECT_CALL(stream_encoder_, encodeData(_, _)).Times(0);
  auto client_setup_parameters = ClientSetupParameters(1, 1, 1, request_generator);
  verifyBenchmarkClientProcessesExpectedInflightRequests(client_setup_parameters);
  EXPECT_EQ(1, getCounter("http_2xx"));
}

TEST_F(BenchmarkClientHttpTest, RequestGeneratorProvidingDifferentPathsSendsRequestsOnThosePaths) {
  std::vector<HeaderMapPtr> requests_for_generator_to_send;
  const std::initializer_list<std::pair<std::string, std::string>> header_map_for_first_request{
      {":scheme", "http"},
      {":method", "GET"},
      {":path", "/a"},
      {":host", "localhost"},
      {"Content-Length", "1313"}};
  const std::initializer_list<std::pair<std::string, std::string>> header_map_for_second_request{
      {":scheme", "http"},
      {":method", "GET"},
      {":path", "/b"},
      {":host", "localhost"},
      {"Content-Length", "1313"}};
  requests_for_generator_to_send.push_back(
      std::make_shared<Envoy::Http::TestRequestHeaderMapImpl>(header_map_for_first_request));
  requests_for_generator_to_send.push_back(
      std::make_shared<Envoy::Http::TestRequestHeaderMapImpl>(header_map_for_second_request));
  std::vector<HeaderMapPtr>::iterator request_iterator;
  request_iterator = requests_for_generator_to_send.begin();
  RequestGenerator request_generator = [&request_iterator]() {
    return std::make_unique<RequestImpl>(*request_iterator++);
  };
  absl::flat_hash_set<std::string> expected_requests;
  expected_requests.insert(
      getPathFromRequest(Envoy::Http::TestRequestHeaderMapImpl(header_map_for_first_request)));
  expected_requests.insert(
      getPathFromRequest(Envoy::Http::TestRequestHeaderMapImpl(header_map_for_second_request)));

  EXPECT_CALL(stream_encoder_, encodeData(_, _)).Times(2);

  // Most of the testing happens inside of this call. Will confirm that the requests received match
  // the expected requests vector.
  auto client_setup_parameters = ClientSetupParameters(1, 1, 2, request_generator);
  verifyBenchmarkClientProcessesExpectedInflightRequests(client_setup_parameters,
                                                         &expected_requests);
  EXPECT_EQ(2, getCounter("http_2xx"));
}

TEST_F(BenchmarkClientHttpTest, DrainTimeoutFires) {
  RequestGenerator default_request_generator = getDefaultRequestGenerator();
  setupBenchmarkClient(default_request_generator);
  EXPECT_CALL(pool_, newStream(_, _))
      .WillOnce(
          [this](Envoy::Http::ResponseDecoder& decoder, Envoy::Http::ConnectionPool::Callbacks&)
              -> Envoy::Http::ConnectionPool::Cancellable* {
            // The decoder self-terminates in normal operation, but in this test that won't
            // happen. Se we delete it ourselves. Note that we run our integration test with
            // asan, so any leaks in real usage ought to be caught there.
            delete &decoder;
            client_->terminate();
            return nullptr;
          });
  EXPECT_CALL(pool_, hasActiveConnections()).WillOnce([]() -> bool { return true; });
  EXPECT_CALL(pool_, addDrainedCallback(_));
  // We don't expect the callback that we pass here to fire.
  client_->tryStartRequest([](bool, bool) { EXPECT_TRUE(false); });
  // To get past this, the drain timeout within the benchmark client must execute.
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
  EXPECT_EQ(0, getCounter("http_2xx"));
}

} // namespace Nighthawk
