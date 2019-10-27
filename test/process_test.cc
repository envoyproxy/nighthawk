#include "nighthawk/common/exception.h"

#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/utility.h"

#include "client/options_impl.h"
#include "client/output_collector_impl.h"
#include "client/process_impl.h"

#include "test/client/utility.h"
#include "test/mocks.h"

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
  void runProcess(RunExpectation expectation) {
    ProcessPtr process = std::make_unique<ProcessImpl>(*options_, time_system_);
    OutputCollectorImpl collector(time_system_, *options_);
    const auto result =
        process->run(collector) ? RunExpectation::EXPECT_SUCCESS : RunExpectation::EXPECT_FAILURE;
    EXPECT_EQ(result, expectation);
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

} // namespace Client
} // namespace Nighthawk
