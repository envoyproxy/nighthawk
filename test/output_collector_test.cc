#include <chrono>

#include "external/envoy/test/test_common/file_system_for_test.h"
#include "external/envoy/test/test_common/simulated_time_system.h"

#include "api/client/options.pb.h"

#include "common/statistic_impl.h"

#include "client/output_collector_impl.h"

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
  }

  void expectEqualToGoldFile(OutputCollectorImpl& collector, absl::string_view path) {
    collector.addResult("worker_0", statistics_, counters_);
    collector.addResult("worker_1", statistics_, counters_);
    collector.addResult("global", statistics_, counters_);
    EXPECT_EQ(Envoy::Filesystem::fileSystemForTest().fileReadToEnd(
                  TestEnvironment::runfilesPath(std::string(path))),
              collector.toString());
  }

  nighthawk::client::CommandLineOptions command_line_options_;
  Envoy::Event::SimulatedTimeSystem time_system_;
  MockOptions options_;
  std::vector<StatisticPtr> statistics_;
  std::map<std::string, uint64_t> counters_;
};

TEST_F(OutputCollectorTest, CliFormatter) {
  ConsoleOutputCollectorImpl collector(time_system_, options_);
  expectEqualToGoldFile(collector, "test/test_data/output_collector.txt.gold");
}

TEST_F(OutputCollectorTest, JsonFormatter) {
  JsonOutputCollectorImpl collector(time_system_, options_);
  expectEqualToGoldFile(collector, "test/test_data/output_collector.json.gold");
}

TEST_F(OutputCollectorTest, YamlFormatter) {
  YamlOutputCollectorImpl collector(time_system_, options_);
  expectEqualToGoldFile(collector, "test/test_data/output_collector.yaml.gold");
}

TEST_F(OutputCollectorTest, DottedFormatter) {
  DottedStringOutputCollectorImpl collector(time_system_, options_);
  expectEqualToGoldFile(collector, "test/test_data/output_collector.dotted.gold");
}

class StatidToNameTest : public Test {};

TEST_F(StatidToNameTest, TestTranslations) {
  // Well known id's shouldn't be returned as-is, but unknown ones should.
  EXPECT_EQ(ConsoleOutputCollectorImpl::statIdtoFriendlyStatName("foo"), "foo");
  const std::vector<std::string> ids = {"benchmark_http_client.queue_to_connect",
                                        "benchmark_http_client.request_to_response",
                                        "sequencer.callback", "sequencer.blocking"};
  for (const std::string& id : ids) {
    EXPECT_NE(ConsoleOutputCollectorImpl::statIdtoFriendlyStatName(id), id);
  }
}

} // namespace Client
} // namespace Nighthawk
