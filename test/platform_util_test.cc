#include <chrono>

#include "common/platform_util_impl.h"

#include "gtest/gtest.h"

using namespace testing;
using namespace std::chrono_literals;

namespace Nighthawk {

class PlatformUtilTest : public Test {
public:
  PlatformUtilImpl platform_util_;
};

TEST_F(PlatformUtilTest, NoFatalFailureForYield) {
  EXPECT_NO_FATAL_FAILURE(platform_util_.yieldCurrentThread());
}

TEST_F(PlatformUtilTest, NoFatalFailureForSleep) {
  EXPECT_NO_FATAL_FAILURE(platform_util_.sleep(1us));
}

} // namespace Nighthawk
