#include "client/process_context_impl.h"

#include "gtest/gtest.h"

#include "client/options_impl.h"
#include "test/client/utility.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

class ProcessContextTest : public Test {};

// TODO(oschaaf): when we have proper integration testing, update this.
// For now we are covered via the client_tests.cc by proxy. Eventually we
// want those tests in here, and mock ProcessContext in client_test.
TEST_F(ProcessContextTest, HelloWorld) {
  OptionsPtr options = Nighthawk::Client::TestUtility::createOptionsImpl(
      fmt::format("foo --address-family v4 --duration 2 --rps 10 http://127.0.0.1/"));

  ProcessContextImpl process_context(*options);
  OutputFormatterFactoryImpl output_format_factory(process_context.time_system(), *options);
  auto formatter = output_format_factory.create();

  EXPECT_TRUE(process_context.run(*formatter));
}

} // namespace Client
} // namespace Nighthawk
