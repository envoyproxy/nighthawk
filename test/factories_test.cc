#include <chrono>

#include "common/api/api_impl.h"
#include "common/uri_impl.h"

#include "client/factories_impl.h"

#include "test/mocks.h"
#include "test/mocks/event/mocks.h"
#include "test/mocks/stats/mocks.h"
#include "test/test_common/simulated_time_system.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

class FactoriesTest : public Test {
public:
  FactoriesTest() : api_(Envoy::Api::createApiForTest(stats_store_)) {}

  Envoy::Api::ApiPtr api_;
  Envoy::Stats::MockIsolatedStatsStore stats_store_;
  Envoy::Event::MockDispatcher dispatcher_;
  MockOptions options_;
};

TEST_F(FactoriesTest, CreateBenchmarkClient) {
  BenchmarkClientFactoryImpl factory(options_);
  EXPECT_CALL(options_, timeout()).Times(1);
  EXPECT_CALL(options_, connections()).Times(1);
  EXPECT_CALL(options_, h2()).Times(1);
  EXPECT_CALL(options_, prefetchConnections()).Times(1);
  EXPECT_CALL(options_, requestMethod()).Times(1);
  EXPECT_CALL(options_, requestBodySize()).Times(1);
  auto cmd = std::make_unique<nighthawk::client::CommandLineOptions>();
  auto request_headers = cmd->mutable_request_options()->add_request_headers();
  request_headers->mutable_header()->set_key("foo");
  request_headers->mutable_header()->set_value("bar");
  EXPECT_CALL(options_, toCommandLineOptions()).Times(1).WillOnce(Return(ByMove(std::move(cmd))));

  auto benchmark_client =
      factory.create(*api_, dispatcher_, stats_store_, std::make_unique<UriImpl>("http://foo/"));
  EXPECT_NE(nullptr, benchmark_client.get());
}

TEST_F(FactoriesTest, CreateSequencer) {
  SequencerFactoryImpl factory(options_);
  MockBenchmarkClient benchmark_client;

  EXPECT_CALL(options_, timeout()).Times(1);
  EXPECT_CALL(options_, duration()).Times(1).WillOnce(Return(1s));
  EXPECT_CALL(options_, requestsPerSecond()).Times(1).WillOnce(Return(1));
  EXPECT_CALL(options_, burstSize()).Times(1).WillOnce(Return(2));
  EXPECT_CALL(dispatcher_, createTimer_(_)).Times(2);
  Envoy::Event::SimulatedTimeSystem time_system;
  auto sequencer = factory.create(api_->timeSource(), dispatcher_, time_system.monotonicTime(),
                                  benchmark_client);
  EXPECT_NE(nullptr, sequencer.get());
}

TEST_F(FactoriesTest, CreateStore) {
  StoreFactoryImpl factory(options_);
  EXPECT_NE(nullptr, factory.create().get());
}

TEST_F(FactoriesTest, CreateStatistic) {
  StatisticFactoryImpl factory(options_);
  EXPECT_NE(nullptr, factory.create().get());
}

class OutputCollectorFactoryTest : public FactoriesTest, public WithParamInterface<const char*> {
public:
  void testOutputCollector(absl::string_view type) {
    Envoy::Event::SimulatedTimeSystem time_source;
    EXPECT_CALL(options_, toCommandLineOptions());
    EXPECT_CALL(options_, outputFormat()).WillOnce(Return(std::string(type)));
    OutputCollectorFactoryImpl factory(time_source, options_);
    EXPECT_NE(nullptr, factory.create().get());
  }
};

TEST_P(OutputCollectorFactoryTest, TestCreation) { testOutputCollector(GetParam()); }

INSTANTIATE_TEST_SUITE_P(OutputFormats, OutputCollectorFactoryTest,
                         ValuesIn({"human", "json", "yaml"}));

} // namespace Client
} // namespace Nighthawk
