#include "nighthawk/common/exception.h"

#include "client/options_impl.h"
#include "client/process_impl.h"

#include "test/client/utility.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

// TODO(oschaaf): when we have proper integration testing, update this.
// For now we are covered via the client_tests.cc by proxy. Eventually we
// want those tests in here, and mock Process in client_test.
class ProcessTest : public Test {
public:
  void runProcess() {

    OptionsPtr options = TestUtility::createOptionsImpl(
        fmt::format("foo --address-family v4 --duration 2 --rps 10 http://127.0.0.1/"));

    ProcessPtr process = std::make_unique<ProcessImpl>(*options, time_system_);
    OutputFormatterFactoryImpl output_format_factory(time_system_, *options);
    auto formatter = output_format_factory.create();

    EXPECT_TRUE(process->run(*formatter));
    process.reset();
  }

  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
};

TEST_F(ProcessTest, TwoProcesssInSequence) {
  runProcess();
  runProcess();
}

TEST_F(ProcessTest, SimultaneousProcessExcepts) {
  OptionsPtr options = TestUtility::createOptionsImpl(fmt::format("foo http://127.0.0.1/"));
  ProcessImpl process(*options, time_system_);
  EXPECT_THROW_WITH_REGEX(runProcess(), NighthawkException,
                          "Only a single ProcessImpl instance is allowed to exist at a time.");
}

} // namespace Client
} // namespace Nighthawk
