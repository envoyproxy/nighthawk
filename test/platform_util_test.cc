#include <chrono>

#include "common/platform_util_impl.h"

#include "gtest/gtest.h"

using namespace testing;
using namespace std::chrono_literals;

namespace Nighthawk {

class PlatformUtilTest : public Test {
public:
  int32_t getCpuCountFromSet(cpu_set_t& set) { return CPU_COUNT(&set); }
  PlatformUtilImpl platform_util_;
};

TEST_F(PlatformUtilTest, NoFatalFailureForYield) {
  EXPECT_NO_FATAL_FAILURE(platform_util_.yieldCurrentThread());
}

TEST_F(PlatformUtilTest, NoFatalFailureForSleep) {
  EXPECT_NO_FATAL_FAILURE(platform_util_.sleep(1us));
}

// TODO(oschaaf): we probably want to move this out to another file.
TEST_F(PlatformUtilTest, CpusWithAffinity) {
  cpu_set_t original_set;
  CPU_ZERO(&original_set);
  EXPECT_EQ(0, sched_getaffinity(0, sizeof(original_set), &original_set));

  uint32_t original_cpu_count = platform_util_.determineCpuCoresWithAffinity();
  EXPECT_EQ(original_cpu_count, getCpuCountFromSet(original_set));

  // Now the test, we set affinity to just the first cpu. We expect that to be reflected.
  // This will be a no-op on a single core system.
  cpu_set_t test_set;
  CPU_ZERO(&test_set);
  CPU_SET(0, &test_set);
  EXPECT_EQ(0, sched_setaffinity(0, sizeof(test_set), &test_set));
  EXPECT_EQ(1, platform_util_.determineCpuCoresWithAffinity());

  // Restore affinity to what it was.
  EXPECT_EQ(0, sched_setaffinity(0, sizeof(original_set), &original_set));
  EXPECT_EQ(original_cpu_count, platform_util_.determineCpuCoresWithAffinity());
}

} // namespace Nighthawk
