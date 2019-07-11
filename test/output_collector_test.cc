#include <chrono>

#include "common/filesystem/filesystem_impl.h"
#include "common/statistic_impl.h"

#include "client/output_collector_impl.h"

#include "test/mocks.h"
#include "test/test_common/environment.h"
#include "test/test_common/simulated_time_system.h"

#include "api/client/options.pb.h"
#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

class OutputCollectorTest : public Test {
public:
  OutputCollectorTest() {
    addStatWithId("stat_id");
    StatisticPtr empty_statistic = std::make_unique<StreamingStatistic>();
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
    EXPECT_EQ(filesystem_.fileReadToEnd(TestEnvironment::runfilesPath(std::string(path))),
              collector.toString());
  }

  void addStatWithId(absl::string_view id) {
    StatisticPtr stat = std::make_unique<StreamingStatistic>();
    stat->setId(id);
    stat->addValue(1000000);
    stat->addValue(2000000);
    stat->addValue(3000000);
    statistics_.push_back(std::move(stat));
  }

  nighthawk::client::CommandLineOptions command_line_options_;
  Envoy::Event::SimulatedTimeSystem time_system_;
  Envoy::Filesystem::InstanceImplPosix filesystem_;
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

TEST_F(OutputCollectorTest, CliFormatterstatIdFiendlyNames) {
  ConsoleOutputCollectorImpl collector(time_system_, options_);
  // Makes us hit switch cases in statIdtoFriendlyStatName()
  addStatWithId("benchmark_http_client.queue_to_connect");
  addStatWithId("benchmark_http_client.request_to_response");
  addStatWithId("sequencer.callback");
  addStatWithId("sequencer.blocking");
  // TODO(oschaaf): one day we may want to ensure that we do get the nice names.
  // For now, I'm happy if this doesn't fail us.
  ASSERT_NO_FATAL_FAILURE(collector.toProto());
}

} // namespace Client
} // namespace Nighthawk
