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
  ASSERT_EQ(0, system(path.c_str()));
}

} // namespace Nighthawk