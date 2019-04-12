#include "gtest/gtest.h"

#include "nighthawk/source/common/platform_util_impl.h"

namespace Nighthawk {

class PlatformUtilTest : public testing::Test {
public:
  PlatformUtilImpl platform_util_;
};

TEST_F(PlatformUtilTest, NoFatalFailureForYield) {
  EXPECT_NO_FATAL_FAILURE(platform_util_.yieldCurrentThread());
}

} // namespace Nighthawk
