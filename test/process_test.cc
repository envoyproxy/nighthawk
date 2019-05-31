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

} // namespace Client
} // namespace Nighthawk
