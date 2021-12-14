#include "test/test_common/environment.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {

class PythonTest : public Test {};

// This runs the python integration tests from within a test context, for the purpose
// of getting code coverage reporting to also consider the code hit by integration tests.
TEST_F(PythonTest, IntegrationTests) {
  const std::string path = TestEnvironment::runfilesPath("test/integration/integration_test");
  const std::string altered_cmd = path + " test_h3_quic";
#if defined(__has_feature) && (__has_feature(thread_sanitizer) || __has_feature(address_sanitizer))
  char env[] = "NH_INTEGRATION_TEST_SANITIZER_RUN=1";
  putenv(env);
#endif
  ASSERT_EQ(0, system(altered_cmd.c_str()));
}

} // namespace Nighthawk
