#include "source/common/frequency.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {

class FrequencyTest : public Test {};

TEST_F(FrequencyTest, BasicTest) {
  Frequency f1 = 1_Hz;
  EXPECT_EQ(1, f1.value());
  EXPECT_EQ(std::chrono::seconds(1), f1.interval());

  Frequency f2 = 1000_kHz;
  EXPECT_EQ(1000 * 1000, f2.value());
  EXPECT_EQ(std::chrono::duration<double>(1.0 / (1000 * 1000)), f2.interval());
}

} // namespace Nighthawk
