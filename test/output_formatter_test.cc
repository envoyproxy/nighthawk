#include <chrono>

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/test/test_common/file_system_for_test.h"
#include "external/envoy/test/test_common/simulated_time_system.h"

#include "api/client/options.pb.h"
#include "api/client/output.pb.h"

#include "common/statistic_impl.h"

#include "client/output_collector_impl.h"
#include "client/output_formatter_impl.h"

#include "test_common/environment.h"

#include "test/mocks.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

class OutputCollectorTest : public Test {
public:
  OutputCollectorTest() {
    StatisticPtr used_statistic = std::make_unique<StreamingStatistic>();
    StatisticPtr empty_statistic = std::make_unique<StreamingStatistic>();
    used_statistic->setId("stat_id");
    used_statistic->addValue(1000000);
    used_statistic->addValue(2000000);
    used_statistic->addValue(3000000);
    statistics_.push_back(std::move(used_statistic));
    statistics_.push_back(std::move(empty_statistic));
    counters_["foo"] = 1;
    counters_["bar"] = 2;
    time_system_.setSystemTime(std::chrono::milliseconds(1234567891567));
    command_line_options_.mutable_duration()->set_seconds(1);
    command_line_options_.mutable_connections()->set_value(0);
    EXPECT_CALL(options_, toCommandLineOptions())
        .WillOnce(Return(ByMove(
            std::make_unique<nighthawk::client::CommandLineOptions>(command_line_options_))));
    setupCollector();
  }

  void expectEqualToGoldFile(absl::string_view output, absl::string_view path) {
    EXPECT_EQ(Envoy::Filesystem::fileSystemForTest().fileReadToEnd(
                  TestEnvironment::runfilesPath(std::string(path))),
              output);
  }

  void setupCollector() {
    collector_ = std::make_unique<OutputCollectorImpl>(time_system_, options_);
    collector_->addResult("worker_0", statistics_, counters_, 1s);
    collector_->addResult("worker_1", statistics_, counters_, 1s);
    collector_->addResult("global", statistics_, counters_, 1s);
  }

  nighthawk::client::CommandLineOptions command_line_options_;
  Envoy::Event::SimulatedTimeSystem time_system_;
  MockOptions options_;
  std::vector<StatisticPtr> statistics_;
  std::map<std::string, uint64_t> counters_;
  OutputCollectorPtr collector_;
};

TEST_F(OutputCollectorTest, CliFormatter) {
  ConsoleOutputFormatterImpl formatter;
  expectEqualToGoldFile(formatter.formatProto(collector_->toProto()),
                        "test/test_data/output_formatter.txt.gold");
}

TEST_F(OutputCollectorTest, JsonFormatter) {
  JsonOutputFormatterImpl formatter;
  expectEqualToGoldFile(formatter.formatProto(collector_->toProto()),
                        "test/test_data/output_formatter.json.gold");
}

TEST_F(OutputCollectorTest, YamlFormatter) {
  YamlOutputFormatterImpl formatter;
  expectEqualToGoldFile(formatter.formatProto(collector_->toProto()),
                        "test/test_data/output_formatter.yaml.gold");
}

TEST_F(OutputCollectorTest, DottedFormatter) {
  DottedStringOutputFormatterImpl formatter;
  expectEqualToGoldFile(formatter.formatProto(collector_->toProto()),
                        "test/test_data/output_formatter.dotted.gold");
}

TEST_F(OutputCollectorTest, getLowerCaseOutputFormats) {
  auto output_formats = OutputFormatterImpl::getLowerCaseOutputFormats();
  // When you're looking at this code you probably just added an output format.
  // This is to point out that you might want to update the list below and add a test above.
  ASSERT_THAT(output_formats, ElementsAre("json", "human", "yaml", "dotted", "fortio"));
}

class FortioOutputCollectorTest : public OutputCollectorTest {
public:
  FortioOutputCollectorTest() {
    counters_["upstream_rq_total"] = 3;
    counters_["benchmark.http_2xx"] = 4;
    StatisticPtr used_statistic = std::make_unique<StreamingStatistic>();
    used_statistic->setId("benchmark_http_client.request_to_response");
    used_statistic->addValue(4000000);
    statistics_.push_back(std::move(used_statistic));
    EXPECT_CALL(options_, toCommandLineOptions())
        .WillOnce(Return(ByMove(
            std::make_unique<nighthawk::client::CommandLineOptions>(command_line_options_))));
    setupCollector();
  }
};

TEST_F(FortioOutputCollectorTest, MissingGlobalResult) {
  nighthawk::client::Output output_proto = collector_->toProto();
  output_proto.clear_results();

  FortioOutputFormatterImpl formatter;
  ASSERT_THROW(formatter.formatProto(output_proto), NighthawkException);
}

TEST_F(FortioOutputCollectorTest, MissingCounter) {
  nighthawk::client::Output output_proto = collector_->toProto();
  output_proto.mutable_results(2)->clear_counters();

  FortioOutputFormatterImpl formatter;
  ASSERT_THROW(formatter.formatProto(output_proto), NighthawkException);
}

TEST_F(FortioOutputCollectorTest, MissingStatistic) {
  nighthawk::client::Output output_proto = collector_->toProto();
  output_proto.mutable_results(2)->clear_statistics();

  FortioOutputFormatterImpl formatter;
  ASSERT_THROW(formatter.formatProto(output_proto), NighthawkException);
}

TEST_F(FortioOutputCollectorTest, NoExceptions) {
  nighthawk::client::Output output_proto = collector_->toProto();

  FortioOutputFormatterImpl formatter;
  ASSERT_NO_THROW(formatter.formatProto(output_proto));
}

class MediumOutputCollectorTest : public OutputCollectorTest {
public:
  nighthawk::client::Output loadProtoFromFile(absl::string_view path) {
    nighthawk::client::Output proto;
    const auto contents = Envoy::Filesystem::fileSystemForTest().fileReadToEnd(
        TestEnvironment::runfilesPath(std::string(path)));
    Envoy::MessageUtil::loadFromJson(contents, proto,
                                     Envoy::ProtobufMessage::getStrictValidationVisitor());
    return proto;
  }
};

TEST_F(MediumOutputCollectorTest, FortioFormatter) {
  const auto input_proto = loadProtoFromFile("test/test_data/output_formatter.medium.proto.gold");

  FortioOutputFormatterImpl formatter;
  expectEqualToGoldFile(formatter.formatProto(input_proto),
                        "test/test_data/output_formatter.medium.fortio.gold");
}

class StatidToNameTest : public Test {};

TEST_F(StatidToNameTest, TestTranslations) {
  // Well known id's shouldn't be returned as-is, but unknown ones should.
  EXPECT_EQ(ConsoleOutputFormatterImpl::statIdtoFriendlyStatName("foo"), "foo");
  const std::vector<std::string> ids = {"benchmark_http_client.queue_to_connect",
                                        "benchmark_http_client.request_to_response",
                                        "sequencer.callback", "sequencer.blocking"};
  for (const std::string& id : ids) {
    EXPECT_NE(ConsoleOutputFormatterImpl::statIdtoFriendlyStatName(id), id);
  }
}

} // namespace Client
} // namespace Nighthawk
