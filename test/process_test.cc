#include "nighthawk/common/exception.h"

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
class ProcessTest : public testing::Test {
public:
  ProcessTest()
      : options_(TestUtility::createOptionsImpl(
            fmt::format("foo --duration 1 -v error --rps 10 https://127.0.0.1/"))){

        };
  void runProcess() {
    ProcessPtr process = std::make_unique<ProcessImpl>(*options_, time_system_);
    OutputCollectorFactoryImpl output_format_factory(time_system_, *options_);
    auto collector = output_format_factory.create();
    EXPECT_TRUE(process->run(*collector));
    process.reset();
  }

  OptionsPtr options_;
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
};

TEST_F(ProcessTest, TwoProcessInSequence) {
  runProcess();
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --h2 --duration 1 --rps 10 https://127.0.0.1/"));
  runProcess();
}

} // namespace Client
} // namespace Nighthawk
