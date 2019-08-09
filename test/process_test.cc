#include "nighthawk/common/exception.h"

#include "client/options_impl.h"
#include "client/process_impl.h"

#include "test/client/utility.h"
#include "test/mocks.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

// TODO(oschaaf): when we have proper integration testing, update this.
// For now we are covered via the client_tests.cc by proxy. Eventually we
// want those tests in here, and mock Process in client_test.
class ProcessTest : public testing::Test {
public:
  void runProcess() {
    OptionsPtr options = TestUtility::createOptionsImpl(
        fmt::format("foo --address-family v4 --duration 2 --rps 10 https://127.0.0.1/"));
    PlatformUtilImpl platform_util;
    ProcessPtr process = std::make_unique<ProcessImpl>(*options, time_system_, platform_util);
    OutputCollectorFactoryImpl output_format_factory(time_system_, *options);
    auto collector = output_format_factory.create();
    EXPECT_TRUE(process->run(*collector));
    process.reset();
  }

  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
};

TEST_F(ProcessTest, TwoProcesssInSequence) {
  runProcess();
  runProcess();
}

TEST_F(ProcessTest, CpuAffinityDetectionFailure) {
  OptionsPtr options = TestUtility::createOptionsImpl(
      fmt::format("foo --address-family v4 --duration 2 --rps 10 http://127.0.0.1/"));
  MockPlatformUtil platform_util;
  ProcessPtr process = std::make_unique<ProcessImpl>(*options, time_system_, platform_util);
  OutputCollectorFactoryImpl output_format_factory(time_system_, *options);
  auto collector = output_format_factory.create();
  // Will return 0, which happens on failure in the implementation.
  EXPECT_CALL(platform_util, determineCpuCoresWithAffinity);
  EXPECT_TRUE(process->run(*collector));
  // TODO(oschaaf): check the proto output that we reflect the concurreny we actually used.
  // I'm not sure we do so right now.
}

} // namespace Client
} // namespace Nighthawk
