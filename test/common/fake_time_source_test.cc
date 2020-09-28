#include "gtest/gtest.h"

#include "test/common/fake_time_source.h"

namespace Nighthawk {
namespace {

TEST(FakeIncrementingMonotonicTimeSource, SystemTimeAlwaysReturnsEpoch) {
  FakeIncrementingMonotonicTimeSource time_source;
  Envoy::SystemTime epoch;
  EXPECT_EQ(time_source.systemTime(), epoch);
  EXPECT_EQ(time_source.systemTime(), epoch);
}

TEST(FakeIncrementingMonotonicTimeSource, MonotonicTimeStartsFromEpoch) {
  FakeIncrementingMonotonicTimeSource time_source;
  Envoy::MonotonicTime epoch;
  Envoy::MonotonicTime time = time_source.monotonicTime();
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(time - epoch).count(), 0);
}

TEST(FakeIncrementingMonotonicTimeSource, MonotonicTimeIncrementsOneSecondPerCall) {
  FakeIncrementingMonotonicTimeSource time_source;
  Envoy::MonotonicTime time1 = time_source.monotonicTime();
  Envoy::MonotonicTime time2 = time_source.monotonicTime();
  Envoy::MonotonicTime time3 = time_source.monotonicTime();
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(time2 - time1).count(), 1);
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(time3 - time2).count(), 1);
}

} // namespace
} // namespace Nighthawk
