#include "nighthawk/common/exception.h"

#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/utility.h"

#include "client/options_impl.h"
#include "client/process_impl.h"

#include "test/client/utility.h"
#include "test/mocks.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

// TODO(oschaaf): when we have proper integration testing, update this.
// For now we are covered via the client_tests.cc by proxy. Eventually we
// want those tests in here, and mock Process in client_test.
class ProcessTest : public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  ProcessTest()
      : loopback_address_(Envoy::Network::Test::getLoopbackAddressUrlString(GetParam())),
        options_(TestUtility::createOptionsImpl(
            fmt::format("foo --duration 1 -v error --rps 10 https://{}/", loopback_address_))){};
  void runProcess() {
    PlatformUtilImpl platform_util;
    ProcessPtr process = std::make_unique<ProcessImpl>(*options_, time_system_, platform_util);
    OutputCollectorFactoryImpl output_format_factory(time_system_, *options_);
    auto collector = output_format_factory.create();
    EXPECT_TRUE(process->run(*collector));
    process.reset();
  }

  const std::string loopback_address_;
  OptionsPtr options_;
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
};

TEST_P(ProcessTest, TwoProcessInSequence) {
  runProcess();
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --h2 --duration 1 --rps 10 https://{}/", loopback_address_));
  runProcess();
}

TEST_P(ProcessTest, CpuAffinityDetectionFailure) {
  MockPlatformUtil platform_util;
  ProcessPtr process = std::make_unique<ProcessImpl>(*options_, time_system_, platform_util);
  OutputCollectorFactoryImpl output_format_factory(time_system_, *options_);
  auto collector = output_format_factory.create();
  // Will return 0, which happens on failure in the implementation.
  EXPECT_CALL(platform_util, determineCpuCoresWithAffinity);
  EXPECT_TRUE(process->run(*collector));
  // TODO(oschaaf): check the proto output that we reflect the concurrency we actually used.
  // I'm not sure we do so right now.
}

} // namespace Client
} // namespace Nighthawk
