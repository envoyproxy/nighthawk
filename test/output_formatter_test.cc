#include <chrono>

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"
#include "external/envoy/test/test_common/file_system_for_test.h"
#include "external/envoy/test/test_common/simulated_time_system.h"

#include "api/client/options.pb.h"
#include "api/client/output.pb.h"

#include "common/statistic_impl.h"
#include "common/version_info.h"

#include "client/output_collector_impl.h"
#include "client/output_formatter_impl.h"

#include "test_common/environment.h"

#include "test/mocks/client/mock_options.h"

#include "absl/strings/str_replace.h"
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
    StatisticPtr size_statistic = std::make_unique<HdrStatistic>();
    StatisticPtr latency_statistic = std::make_unique<HdrStatistic>();

    used_statistic->setId("stat_id");
    used_statistic->addValue(1000000);
    used_statistic->addValue(2000000);
    used_statistic->addValue(3000000);

    size_statistic->addValue(14);
    size_statistic->addValue(15);
    size_statistic->addValue(16);
    size_statistic->addValue(17);
    size_statistic->setId("foo_size");

    latency_statistic->addValue(180000);
    latency_statistic->addValue(190000);
    latency_statistic->addValue(200000);
    latency_statistic->addValue(210000);
    latency_statistic->setId("foo_latency");

    statistics_.push_back(std::move(used_statistic));
    statistics_.push_back(std::move(empty_statistic));
    statistics_.push_back(std::move(size_statistic));
    statistics_.push_back(std::move(latency_statistic));

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
    std::string s = Envoy::Filesystem::fileSystemForTest().fileReadToEnd(
        TestEnvironment::runfilesPath(std::string(path)));
    const auto version = VersionInfo::buildVersion().version();
    const std::string major = fmt::format("{}", version.major_number());
    const std::string minor = fmt::format("{}", version.minor_number());
    const std::string patch = fmt::format("{}", version.patch());
    s = absl::StrReplaceAll(s, {{"@version_major@", major}});
    s = absl::StrReplaceAll(s, {{"@version_minor@", minor}});
    s = absl::StrReplaceAll(s, {{"@version_patch@", patch}});
    EXPECT_EQ(s, output);
  }

  void setupCollector() {
    collector_ = std::make_unique<OutputCollectorImpl>(time_system_, options_);
    collector_->addResult("worker_0", statistics_, counters_, 1s, time_system_.systemTime());
    collector_->addResult("worker_1", statistics_, counters_, 1s, absl::nullopt);
    collector_->addResult("global", statistics_, counters_, 1s, time_system_.systemTime());
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

TEST_F(OutputCollectorTest, GetLowerCaseOutputFormats) {
  auto output_formats = OutputFormatterImpl::getLowerCaseOutputFormats();
  // When you're looking at this code you probably just added an output format.
  // This is to point out that you might want to update the list below and add a test above.
  ASSERT_THAT(output_formats, ElementsAre("json", "human", "yaml", "dotted", "fortio",
                                          "experimental_fortio_pedantic"));
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
  ASSERT_NO_THROW(formatter.formatProto(output_proto));
}

TEST_F(FortioOutputCollectorTest, MissingStatistic) {
  nighthawk::client::Output output_proto = collector_->toProto();
  output_proto.mutable_results(2)->clear_statistics();
  FortioOutputFormatterImpl formatter;
  ASSERT_NO_THROW(formatter.formatProto(output_proto));
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
  const nighthawk::client::Output input_proto =
      loadProtoFromFile("test/test_data/output_formatter.medium.proto.gold");
  FortioOutputFormatterImpl formatter;
  expectEqualToGoldFile(formatter.formatProto(input_proto),
                        "test/test_data/output_formatter.medium.fortio.gold");
}

TEST_F(MediumOutputCollectorTest, FortioFormatter0sJitterUniformGetsReflected) {
  nighthawk::client::Output input_proto =
      loadProtoFromFile("test/test_data/output_formatter.medium.proto.gold");
  FortioOutputFormatterImpl formatter;
  input_proto.mutable_options()->mutable_jitter_uniform()->set_nanos(0);
  input_proto.mutable_options()->mutable_jitter_uniform()->set_seconds(0);
  EXPECT_NE(formatter.formatProto(input_proto).find(" \"Jitter\": false,"), std::string::npos);
}

TEST_F(MediumOutputCollectorTest, ConsoleOutputFormatter) {
  const nighthawk::client::Output input_proto =
      loadProtoFromFile("test/test_data/percentile-column-overflow.json");
  ConsoleOutputFormatterImpl formatter;
  expectEqualToGoldFile(formatter.formatProto(input_proto),
                        "test/test_data/percentile-column-overflow.txt.gold");
}

class StatidToNameTest : public Test {};

TEST_F(StatidToNameTest, TestTranslations) {
  // Well known id's shouldn't be returned as-is, but unknown ones should.
  EXPECT_EQ(ConsoleOutputFormatterImpl::statIdtoFriendlyStatName("foo"), "foo");
  const std::vector<std::string> ids = {"benchmark_http_client.queue_to_connect",
                                        "benchmark_http_client.request_to_response",
                                        "benchmark_http_client.response_body_size",
                                        "benchmark_http_client.response_header_size",
                                        "sequencer.callback",
                                        "sequencer.blocking"};
  for (const std::string& id : ids) {
    EXPECT_NE(ConsoleOutputFormatterImpl::statIdtoFriendlyStatName(id), id);
  }
}

TEST_F(MediumOutputCollectorTest, FortioPedanticFormatter) {
  const nighthawk::client::Output input_proto =
      loadProtoFromFile("test/test_data/output_formatter.medium.proto.gold");
  FortioPedanticOutputFormatterImpl formatter;
  expectEqualToGoldFile(formatter.formatProto(input_proto),
                        "test/test_data/output_formatter.medium.fortio-noquirks.gold");
}

} // namespace Client
} // namespace Nighthawk
