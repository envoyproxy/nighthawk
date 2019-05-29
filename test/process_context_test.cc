#include "nighthawk/common/exception.h"

#include "client/options_impl.h"
#include "client/process_context_impl.h"

#include "test/client/utility.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

// TODO(oschaaf): when we have proper integration testing, update this.
// For now we are covered via the client_tests.cc by proxy. Eventually we
// want those tests in here, and mock ProcessContext in client_test.
class ProcessContextTest : public Test {
public:
  void runProcessContext() {
    OptionsPtr options = TestUtility::createOptionsImpl(
        fmt::format("foo --address-family v4 --duration 2 --rps 10 http://127.0.0.1/"));

    ProcessContextPtr process_context = std::make_unique<ProcessContextImpl>(*options);
    OutputFormatterFactoryImpl output_format_factory(process_context->time_system(), *options);
    auto formatter = output_format_factory.create();

    EXPECT_TRUE(process_context->run(*formatter));
    process_context.reset();
  }
};

TEST_F(ProcessContextTest, TwoProcessContextsInSequence) {
  runProcessContext();
  runProcessContext();
}

TEST_F(ProcessContextTest, SimultaneousProcessContextExcepts) {
  OptionsPtr options = TestUtility::createOptionsImpl(fmt::format("foo http://127.0.0.1/"));
  ProcessContextImpl process_context(*options);
  EXPECT_THROW_WITH_REGEX(
      runProcessContext(), NighthawkException,
      "Only a single ProcessContextImpl instance is allowed to exist at a time.");
}

} // namespace Client
} // namespace Nighthawk
