#include "external/envoy/test/mocks/event/mocks.h"
#include "external/envoy/test/mocks/stats/mocks.h"
#include "external/envoy/test/mocks/tracing/mocks.h"
#include "external/envoy/test/test_common/simulated_time_system.h"
#include "external/envoy/test/test_common/utility.h"

#include "common/request_source_impl.h"

#include "client/factories_impl.h"

#include "test/mocks/client/mock_benchmark_client.h"
#include "test/mocks/client/mock_options.h"
#include "test/mocks/common/mock_termination_predicate.h"
#include "test/test_common/environment.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

class FactoriesTest : public Test {
public:
  FactoriesTest()
      : api_(Envoy::Api::createApiForTest(stats_store_)),
        http_tracer_(std::make_unique<Envoy::Tracing::MockHttpTracer>()) {}

  Envoy::Api::ApiPtr api_;
  Envoy::Stats::MockIsolatedStatsStore stats_store_;
  Envoy::Event::MockDispatcher dispatcher_;
  MockOptions options_;
  Envoy::Tracing::HttpTracerSharedPtr http_tracer_;
};

TEST_F(FactoriesTest, CreateBenchmarkClient) {
  BenchmarkClientFactoryImpl factory(options_);
  Envoy::Upstream::ClusterManagerPtr cluster_manager;
  EXPECT_CALL(options_, connections()).Times(1);
  EXPECT_CALL(options_, h2()).Times(1);
  EXPECT_CALL(options_, maxPendingRequests()).Times(1);
  EXPECT_CALL(options_, maxActiveRequests()).Times(1);
  EXPECT_CALL(options_, maxRequestsPerConnection()).Times(1);
  EXPECT_CALL(options_, openLoop()).Times(1);
  EXPECT_CALL(options_, responseHeaderWithLatencyInput()).Times(1);
  auto cmd = std::make_unique<nighthawk::client::CommandLineOptions>();
  EXPECT_CALL(options_, toCommandLineOptions()).Times(1).WillOnce(Return(ByMove(std::move(cmd))));
  StaticRequestSourceImpl request_generator(
      std::make_unique<Envoy::Http::TestRequestHeaderMapImpl>());
  auto benchmark_client =
      factory.create(*api_, dispatcher_, stats_store_, cluster_manager, http_tracer_, "foocluster",
                     /*worker_id=*/0, request_generator);
  EXPECT_NE(nullptr, benchmark_client.get());
}

TEST_F(FactoriesTest, CreateRequestSourcePluginWithWorkingJsonReturnsWorkingRequestSource) {
  absl::optional<envoy::config::core::v3::TypedExtensionConfig> request_source_plugin_config;
  std::string request_source_plugin_config_json =
      "{"
      "name:\"nighthawk.in-line-options-list-request-source-plugin\","
      "typed_config:{"
      "\"@type\":\"type.googleapis.com/"
      "nighthawk.request_source.InLineOptionsListRequestSourceConfig\","
      "options_list:{"
      "options:[{request_method:\"1\",request_headers:[{header:{key:\":path\",value:\"inlinepath\"}"
      "}]}]"
      "},"
      "}"
      "}";
  request_source_plugin_config.emplace(envoy::config::core::v3::TypedExtensionConfig());
  Envoy::MessageUtil::loadFromJson(request_source_plugin_config_json,
                                   request_source_plugin_config.value(),
                                   Envoy::ProtobufMessage::getStrictValidationVisitor());
  EXPECT_CALL(options_, requestMethod()).Times(1);
  EXPECT_CALL(options_, requestBodySize()).Times(1);
  EXPECT_CALL(options_, uri()).Times(2).WillRepeatedly(Return("http://foo/"));
  EXPECT_CALL(options_, requestSource()).Times(1);
  EXPECT_CALL(options_, requestSourcePluginConfig())
      .Times(2)
      .WillRepeatedly(ReturnRef(request_source_plugin_config));
  auto cmd = std::make_unique<nighthawk::client::CommandLineOptions>();
  envoy::config::core::v3::HeaderValueOption* request_headers =
      cmd->mutable_request_options()->add_request_headers();
  request_headers->mutable_header()->set_key("foo");
  request_headers->mutable_header()->set_value("bar");
  EXPECT_CALL(options_, toCommandLineOptions()).Times(1).WillOnce(Return(ByMove(std::move(cmd))));
  RequestSourceFactoryImpl factory(options_, *api_);
  Envoy::Upstream::ClusterManagerPtr cluster_manager;
  Nighthawk::RequestSourcePtr request_source = factory.create(
      cluster_manager, dispatcher_, *stats_store_.createScope("foo."), "requestsource");
  EXPECT_NE(nullptr, request_source.get());
  Nighthawk::RequestGenerator generator = request_source->get();
  Nighthawk::RequestPtr request = generator();
  EXPECT_EQ("inlinepath", request->header()->getPathValue());
}

TEST_F(FactoriesTest, CreateRequestSourcePluginWithNonWorkingJsonThrowsError) {
  absl::optional<envoy::config::core::v3::TypedExtensionConfig> request_source_plugin_config;
  std::string request_source_plugin_config_json =
      "{"
      R"(name:"nighthawk.file-based-request-source-plugin",)"
      "typed_config:{"
      R"("@type":"type.googleapis.com/)"
      R"(nighthawk.request_source.FileBasedOptionsListRequestSourceConfig",)"
      R"(file_path:")" +
      TestEnvironment::runfilesPath("test/request_source/test_data/NotARealFile.yaml") +
      "\","
      "}"
      "}";
  request_source_plugin_config.emplace(envoy::config::core::v3::TypedExtensionConfig());
  Envoy::MessageUtil::loadFromJson(request_source_plugin_config_json,
                                   request_source_plugin_config.value(),
                                   Envoy::ProtobufMessage::getStrictValidationVisitor());
  EXPECT_CALL(options_, requestMethod()).Times(1);
  EXPECT_CALL(options_, requestBodySize()).Times(1);
  EXPECT_CALL(options_, uri()).Times(2).WillRepeatedly(Return("http://foo/"));
  EXPECT_CALL(options_, requestSource()).Times(1);
  EXPECT_CALL(options_, requestSourcePluginConfig())
      .Times(2)
      .WillRepeatedly(ReturnRef(request_source_plugin_config));
  auto cmd = std::make_unique<nighthawk::client::CommandLineOptions>();
  envoy::config::core::v3::HeaderValueOption* request_headers =
      cmd->mutable_request_options()->add_request_headers();
  request_headers->mutable_header()->set_key("foo");
  request_headers->mutable_header()->set_value("bar");
  EXPECT_CALL(options_, toCommandLineOptions()).Times(1).WillOnce(Return(ByMove(std::move(cmd))));
  RequestSourceFactoryImpl factory(options_, *api_);
  Envoy::Upstream::ClusterManagerPtr cluster_manager;
  EXPECT_THROW_WITH_REGEX(
      factory.create(cluster_manager, dispatcher_, *stats_store_.createScope("foo."),
                     "requestsource"),
      NighthawkException,
      "Request Source plugin loading error should have been caught during input validation");
}

TEST_F(FactoriesTest, CreateRequestSource) {
  absl::optional<envoy::config::core::v3::TypedExtensionConfig> request_source_plugin_config;
  EXPECT_CALL(options_, requestMethod()).Times(1);
  EXPECT_CALL(options_, requestBodySize()).Times(1);
  EXPECT_CALL(options_, uri()).Times(2).WillRepeatedly(Return("http://foo/"));
  EXPECT_CALL(options_, requestSource()).Times(1);
  EXPECT_CALL(options_, requestSourcePluginConfig())
      .Times(1)
      .WillRepeatedly(ReturnRef(request_source_plugin_config));
  auto cmd = std::make_unique<nighthawk::client::CommandLineOptions>();
  envoy::config::core::v3::HeaderValueOption* request_headers =
      cmd->mutable_request_options()->add_request_headers();
  request_headers->mutable_header()->set_key("foo");
  request_headers->mutable_header()->set_value("bar");
  EXPECT_CALL(options_, toCommandLineOptions()).Times(1).WillOnce(Return(ByMove(std::move(cmd))));
  RequestSourceFactoryImpl factory(options_, *api_);
  Envoy::Upstream::ClusterManagerPtr cluster_manager;
  RequestSourcePtr request_generator = factory.create(
      cluster_manager, dispatcher_, *stats_store_.createScope("foo."), "requestsource");
  EXPECT_NE(nullptr, request_generator.get());
}

TEST_F(FactoriesTest, CreateRemoteRequestSource) {
  absl::optional<envoy::config::core::v3::TypedExtensionConfig> request_source_plugin_config;
  EXPECT_CALL(options_, requestMethod()).Times(1);
  EXPECT_CALL(options_, requestBodySize()).Times(1);
  EXPECT_CALL(options_, uri()).Times(2).WillRepeatedly(Return("http://foo/"));
  EXPECT_CALL(options_, requestSource()).Times(1).WillRepeatedly(Return("http://bar/"));
  EXPECT_CALL(options_, requestsPerSecond()).Times(1).WillRepeatedly(Return(5));
  auto cmd = std::make_unique<nighthawk::client::CommandLineOptions>();
  envoy::config::core::v3::HeaderValueOption* request_headers =
      cmd->mutable_request_options()->add_request_headers();
  request_headers->mutable_header()->set_key("foo");
  request_headers->mutable_header()->set_value("bar");
  EXPECT_CALL(options_, toCommandLineOptions()).Times(1).WillOnce(Return(ByMove(std::move(cmd))));
  RequestSourceFactoryImpl factory(options_, *api_);
  Envoy::Upstream::ClusterManagerPtr cluster_manager;
  RequestSourcePtr request_generator = factory.create(
      cluster_manager, dispatcher_, *stats_store_.createScope("foo."), "requestsource");
  EXPECT_NE(nullptr, request_generator.get());
}

TEST_F(FactoriesTest, CreateSequencer) {}
class SequencerFactoryTest
    : public FactoriesTest,
      public WithParamInterface<
          nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions> {
public:
  void testSequencerCreation(nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions
                                 sequencer_idle_strategy) {
    SequencerFactoryImpl factory(options_);
    MockBenchmarkClient benchmark_client;
    EXPECT_CALL(options_, requestsPerSecond()).Times(1).WillOnce(Return(1));
    EXPECT_CALL(options_, burstSize()).Times(1).WillOnce(Return(2));
    EXPECT_CALL(options_, sequencerIdleStrategy())
        .Times(1)
        .WillOnce(Return(sequencer_idle_strategy));
    EXPECT_CALL(dispatcher_, createTimer_(_)).Times(2);
    EXPECT_CALL(options_, jitterUniform()).Times(1).WillOnce(Return(1ns));
    Envoy::Event::SimulatedTimeSystem time_system;
    const SequencerTarget dummy_sequencer_target = [](const CompletionCallback&) -> bool {
      return true;
    };
    auto sequencer = factory.create(api_->timeSource(), dispatcher_, dummy_sequencer_target,
                                    std::make_unique<MockTerminationPredicate>(), stats_store_,
                                    time_system.monotonicTime() + 10ms);
    EXPECT_NE(nullptr, sequencer.get());
  }
};

TEST_P(SequencerFactoryTest, TestCreation) { testSequencerCreation(GetParam()); }

INSTANTIATE_TEST_SUITE_P(SequencerIdleStrategies, SequencerFactoryTest,
                         ValuesIn({nighthawk::client::SequencerIdleStrategy::POLL,
                                   nighthawk::client::SequencerIdleStrategy::SLEEP,
                                   nighthawk::client::SequencerIdleStrategy::SPIN}));

TEST_F(FactoriesTest, CreateStatistic) {
  StatisticFactoryImpl factory(options_);
  EXPECT_NE(nullptr, factory.create().get());
}

class OutputFormatterFactoryTest
    : public FactoriesTest,
      public WithParamInterface<nighthawk::client::OutputFormat::OutputFormatOptions> {
public:
  void testOutputCollector(nighthawk::client::OutputFormat::OutputFormatOptions type) {
    Envoy::Event::SimulatedTimeSystem time_source;
    EXPECT_CALL(options_, outputFormat()).WillOnce(Return(type));
    OutputFormatterFactoryImpl factory;
    EXPECT_NE(nullptr, factory.create(options_.outputFormat()).get());
  }
};

TEST_P(OutputFormatterFactoryTest, TestCreation) { testOutputCollector(GetParam()); }

INSTANTIATE_TEST_SUITE_P(
    OutputFormats, OutputFormatterFactoryTest,
    ValuesIn({nighthawk::client::OutputFormat::HUMAN, nighthawk::client::OutputFormat::JSON,
              nighthawk::client::OutputFormat::YAML, nighthawk::client::OutputFormat::DOTTED,
              nighthawk::client::OutputFormat::FORTIO}));

} // namespace Client
} // namespace Nighthawk
