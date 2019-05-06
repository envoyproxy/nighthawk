#include "common/platform_util_impl.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {

class PlatformUtilTest : public Test {
public:
  PlatformUtilImpl platform_util_;
};

TEST_F(PlatformUtilTest, NoFatalFailureForYield) {
  EXPECT_NO_FATAL_FAILURE(platform_util_.yieldCurrentThread());
}

} // namespace Nighthawk
