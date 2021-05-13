#include "test/common/fake_time_source.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

TEST(FakeIncrementingMonotonicTimeSource, SystemTimeStartsFromEpoch) {
  FakeIncrementingMonotonicTimeSource time_source;
  Envoy::SystemTime epoch;
  Envoy::SystemTime time = time_source.systemTime();
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(time - epoch).count(), 0);
}

TEST(FakeIncrementingMonotonicTimeSource, SystemTimeIncrementsOneSecondPerCall) {
  FakeIncrementingMonotonicTimeSource time_source;
  Envoy::SystemTime time1 = time_source.systemTime();
  Envoy::SystemTime time2 = time_source.systemTime();
  Envoy::SystemTime time3 = time_source.systemTime();
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(time2 - time1).count(), 1);
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(time3 - time2).count(), 1);
}

TEST(FakeIncrementingMonotonicTimeSource, SetsSystemTimeSecondsThenIncrementsOneSecondPerCall) {
  FakeIncrementingMonotonicTimeSource time_source;
  time_source.setSystemTimeSeconds(10);
  Envoy::SystemTime time1 = time_source.systemTime();
  Envoy::SystemTime time2 = time_source.systemTime();
  EXPECT_EQ(time1.time_since_epoch(), std::chrono::seconds(10));
  EXPECT_EQ(time2.time_since_epoch(), std::chrono::seconds(11));
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

TEST(FakeIncrementingMonotonicTimeSource, SetsMonotonicTimeSecondsThenIncrementsOneSecondPerCall) {
  FakeIncrementingMonotonicTimeSource time_source;
  time_source.setMonotonicTimeSeconds(10);
  Envoy::MonotonicTime time1 = time_source.monotonicTime();
  Envoy::MonotonicTime time2 = time_source.monotonicTime();
  EXPECT_EQ(time1.time_since_epoch(), std::chrono::seconds(10));
  EXPECT_EQ(time2.time_since_epoch(), std::chrono::seconds(11));
}

} // namespace
} // namespace Nighthawk
