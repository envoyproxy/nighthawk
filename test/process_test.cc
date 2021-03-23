#include <chrono>
#include <thread>
#include <vector>

#include "nighthawk/common/exception.h"

#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/registry.h"
#include "external/envoy/test/test_common/simulated_time_system.h"
#include "external/envoy/test/test_common/utility.h"

#include "common/uri_impl.h"

#include "client/options_impl.h"
#include "client/output_collector_impl.h"
#include "client/process_impl.h"

#include "test/client/utility.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace Client {
namespace {

using ::testing::TestWithParam;
using ::testing::ValuesIn;

constexpr absl::string_view kSinkName = "{name:\"nighthawk.fake_stats_sink\"}";
// Global variable keeps count of number of flushes in FakeStatsSink. It is reset
// to 0 when a new FakeStatsSink is created.
int numFlushes = 0;

// FakeStatsSink is a simple Envoy::Stats::Sink implementation used to prove
// the logic to configure Sink in Nighthawk works as expected.
class FakeStatsSink : public Envoy::Stats::Sink {
public:
  FakeStatsSink() { numFlushes = 0; }

  // Envoy::Stats::Sink
  void flush(Envoy::Stats::MetricSnapshot&) override { numFlushes++; }

  void onHistogramComplete(const Envoy::Stats::Histogram&, uint64_t) override {}
};

// FakeStatsSinkFactory creates FakeStatsSink.
class FakeStatsSinkFactory : public NighthawkStatsSinkFactory {
public:
  std::unique_ptr<Envoy::Stats::Sink> createStatsSink(Envoy::Stats::SymbolTable&) override {
    return std::make_unique<FakeStatsSink>();
  }

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    // Using Struct instead of a custom per-filter empty config proto
    // This is only allowed in tests.
    return Envoy::ProtobufTypes::MessagePtr{new Envoy::ProtobufWkt::Struct()};
  }

  std::string name() const override { return "nighthawk.fake_stats_sink"; }
};

// TODO(https://github.com/envoyproxy/nighthawk/issues/179): Mock Process in client_test, and move
// it's tests in here. Note: these tests do not have a backend set up to talk to. That's why we
// expect failure.
class ProcessTest : public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  enum class RunExpectation { EXPECT_SUCCESS, EXPECT_FAILURE };

  ProcessTest()
      : loopback_address_(Envoy::Network::Test::getLoopbackAddressUrlString(GetParam())),
        options_(TestUtility::createOptionsImpl(
            fmt::format("foo --duration 1 -v error --rps 10 https://{}/", loopback_address_))){};

  void runProcess(RunExpectation expectation, bool do_cancel = false,
                  bool terminate_right_away = false) {
    ProcessPtr process = std::make_unique<ProcessImpl>(*options_, time_system_);
    OutputCollectorImpl collector(time_system_, *options_);
    std::thread cancel_thread;
    if (do_cancel) {
      cancel_thread = std::thread([&process, terminate_right_away] {
        if (!terminate_right_away) {
          // We sleep to give the the load test execution in the other thread a change to get
          // started before we request cancellation. Five seconds has been determined to work with
          // the sanitizer runs in CI through emperical observation.
          sleep(5);
        }
        process->requestExecutionCancellation();
      });
      if (terminate_right_away) {
        cancel_thread.join();
      }
    }
    const auto result =
        process->run(collector) ? RunExpectation::EXPECT_SUCCESS : RunExpectation::EXPECT_FAILURE;
    EXPECT_EQ(result, expectation);
    if (do_cancel) {
      if (cancel_thread.joinable()) {
        cancel_thread.join();
      }
      auto proto = collector.toProto();
      if (terminate_right_away) {
        EXPECT_EQ(0, proto.results().size());
      } else {
        int graceful_stop_requested = 0;
        for (const auto& result : proto.results()) {
          for (const auto& counter : result.counters()) {
            if (counter.name() == "graceful_stop_requested") {
              graceful_stop_requested++;
            }
          }
        }
        EXPECT_EQ(3, graceful_stop_requested); // global results + two workers
      }
    }
    process->shutdown();
  }

  const std::string loopback_address_;
  OptionsPtr options_;
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ProcessTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(ProcessTest, TwoProcessInSequence) {
  runProcess(RunExpectation::EXPECT_FAILURE);
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --h2 --duration 1 --rps 10 https://{}/", loopback_address_));
  runProcess(RunExpectation::EXPECT_FAILURE);
}

// TODO(oschaaf): move to python int. tests once it adds to coverage.
TEST_P(ProcessTest, BadTracerSpec) {
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --trace foo://localhost:79/api/v1/spans https://{}/", loopback_address_));
  runProcess(RunExpectation::EXPECT_FAILURE);
}

TEST_P(ProcessTest, CancelDuringLoadTest) {
  // The failure predicate below is there to wipe out any stock ones. We want this to run for a long
  // time, even if the upstream fails (there is no live upstream in this test, we send traffic into
  // the void), so we can check cancellation works.
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --duration 300 --failure-predicate foo:0 --concurrency 2 https://{}/",
                  loopback_address_));
  runProcess(RunExpectation::EXPECT_SUCCESS, true);
}

TEST_P(ProcessTest, CancelExecutionBeforeBeginLoadTest) {
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --duration 300 --failure-predicate foo:0 --concurrency 2 https://{}/",
                  loopback_address_));
  runProcess(RunExpectation::EXPECT_SUCCESS, true, true);
}

TEST_P(ProcessTest, RunProcessWithStatsSinkConfigured) {
  FakeStatsSinkFactory factory;
  Envoy::Registry::InjectFactory<NighthawkStatsSinkFactory> registered(factory);
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --h2 --duration 1 --rps 10 --stats-flush-interval 1 "
                  "--stats-sinks {} https://{}/",
                  kSinkName, loopback_address_));
  numFlushes = 0;
  runProcess(RunExpectation::EXPECT_FAILURE);
  EXPECT_GT(numFlushes, 0);
}

TEST_P(ProcessTest, NoFlushWhenCancelExecutionBeforeLoadTestBegin) {
  FakeStatsSinkFactory factory;
  Envoy::Registry::InjectFactory<NighthawkStatsSinkFactory> registered(factory);
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --duration 300 --failure-predicate foo:0 --concurrency "
                  "2 --stats-flush-interval 1 --stats-sinks {} https://{}/",
                  kSinkName, loopback_address_));
  numFlushes = 0;
  runProcess(RunExpectation::EXPECT_SUCCESS, true, true);
  EXPECT_EQ(numFlushes, 0);
}

/**
 * Fixture for executing the Nighthawk process with simulated time.
 */
class ProcessTestWithSimTime : public Envoy::Event::TestUsingSimulatedTime,
                               public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  ProcessTestWithSimTime()
      : options_(TestUtility::createOptionsImpl(
            fmt::format("foo --duration 1 -v error --failure-predicate foo:0 --rps 10 https://{}/",
                        Envoy::Network::Test::getLoopbackAddressUrlString(GetParam())))){};

protected:
  void run(std::function<void(bool, const nighthawk::client::Output&)> verify_callback) {
    auto run_thread = std::thread([this, &verify_callback] {
      ProcessPtr process = std::make_unique<ProcessImpl>(*options_, simTime());
      OutputCollectorImpl collector(simTime(), *options_);
      const bool result = process->run(collector);
      process->shutdown();
      verify_callback(result, collector.toProto());
    });

    // We introduce real-world sleeps to give the executing ProcessImpl
    // an opportunity to observe passage of simulated time. We increase simulated
    // time in three steps, to give it an opportunity to start at the wrong time
    // in case there is an error in the scheduling logic it implements.
    // Note that these sleeps may seem excessively long, but sanitizer runs may need that.
    sleep(1);
    // Move time to 1 second before the scheduled execution time.
    simTime().setSystemTime(options_->scheduled_start().value() - 1s);
    sleep(1);
    // Move time right up to the scheduled execution time.
    simTime().setSystemTime(options_->scheduled_start().value());
    sleep(1);
    // Move time past the scheduled execution time and execution duration.
    simTime().setSystemTime(options_->scheduled_start().value() + 2s);
    // Wait for execution to wrap up.
    run_thread.join();
  }

  void setScheduleOnOptions(std::chrono::nanoseconds ns_since_epoch) {
    CommandLineOptionsPtr command_line = options_->toCommandLineOptions();
    *(command_line->mutable_scheduled_start()) =
        Envoy::Protobuf::util::TimeUtil::NanosecondsToTimestamp(ns_since_epoch.count());
    options_ = std::make_unique<OptionsImpl>(*command_line);
  }

  OptionsPtr options_;
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ProcessTestWithSimTime,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

// Verify that scheduling execution ahead of time works, and that the execution start timestamp
// associated to the worker result correctly reflects the scheduled time. This should be spot on
// because we use simulated time.
TEST_P(ProcessTestWithSimTime, ScheduleAheadWorks) {
  for (const auto& relative_schedule : std::vector<std::chrono::nanoseconds>{30s, 1h}) {
    setScheduleOnOptions(
        std::chrono::nanoseconds(simTime().systemTime().time_since_epoch() + relative_schedule));
    run([this](bool success, const nighthawk::client::Output& output) {
      EXPECT_TRUE(success);
      ASSERT_EQ(output.results_size(), 1);
      EXPECT_EQ(Envoy::ProtobufUtil::TimeUtil::TimestampToNanoseconds(
                    output.results()[0].execution_start()),
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    options_->scheduled_start().value().time_since_epoch())
                    .count());
    });
  }
}

// Verify that scheduling an execution in the past yields an error.
TEST_P(ProcessTestWithSimTime, ScheduleInThePastFails) {
  setScheduleOnOptions(std::chrono::nanoseconds(simTime().systemTime().time_since_epoch() - 1s));
  run([](bool success, const nighthawk::client::Output& output) {
    EXPECT_FALSE(success);
    EXPECT_EQ(output.results_size(), 0);
  });
}

} // namespace
} // namespace Client
} // namespace Nighthawk
