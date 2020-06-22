#include <thread>
#include <vector>

#include "nighthawk/common/exception.h"

#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/utility.h"

#include "common/uri_impl.h"

#include "client/options_impl.h"
#include "client/output_collector_impl.h"
#include "client/process_impl.h"

#include "test/client/utility.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

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

  void runProcess(RunExpectation expectation, bool do_cancel = false) {
    ProcessPtr process = std::make_unique<ProcessImpl>(*options_, time_system_);
    OutputCollectorImpl collector(time_system_, *options_);
    std::thread cancel_thread;
    if (do_cancel) {
      cancel_thread = std::thread([&process] {
        sleep(5);
        process->requestExecutionCancellation();
      });
    }
    const auto result =
        process->run(collector) ? RunExpectation::EXPECT_SUCCESS : RunExpectation::EXPECT_FAILURE;
    EXPECT_EQ(result, expectation);
    if (cancel_thread.joinable()) {
      cancel_thread.join();
    }
    if (do_cancel) {
      auto proto = collector.toProto();
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

TEST_P(ProcessTest, CancelExecution) {
  // The failure predicate below is there to wipe out any stock ones. We want this to run for a long
  // time, even if the upstream fails (there is no live upstream in this test, we send traffic into
  // the void), so we can check cancellation works.
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --duration 300 --failure-predicate foo:0 --concurrency 2 https://{}/",
                  loopback_address_));
  runProcess(RunExpectation::EXPECT_SUCCESS, true);
}

} // namespace Client
} // namespace Nighthawk
