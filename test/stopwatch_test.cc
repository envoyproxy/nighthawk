#include "external/envoy/test/test_common/simulated_time_system.h"
#include "external/envoy/test/test_common/utility.h"

#include "common/thread_safe_monotonic_time_stopwatch.h"

#include "gtest/gtest.h"

namespace Nighthawk {

using namespace std::chrono_literals;

class StopwatchTest : public testing::Test, public Envoy::Event::TestUsingSimulatedTime {};

TEST_F(StopwatchTest, TestElapsedAndReset) {
  ThreadSafeMontonicTimeStopwatch stopwatch;
  Envoy::Event::SimulatedTimeSystem& time_system = simTime();
  time_system.setMonotonicTime(1ns);
  // The first call should always return 0.
  EXPECT_EQ(stopwatch.getElapsedNsAndReset(time_system), 0);
  time_system.setMonotonicTime(2ns);
  // Verify that moving the clock yields correct results.
  EXPECT_EQ(stopwatch.getElapsedNsAndReset(time_system), 1);
  time_system.setMonotonicTime(3ns);
  EXPECT_EQ(stopwatch.getElapsedNsAndReset(time_system), 1);
  time_system.setMonotonicTime(5ns);
  EXPECT_EQ(stopwatch.getElapsedNsAndReset(time_system), 2);
}

// TODO(oschaaf): test spamming a single stopwatch from multiple threads.

} // namespace Nighthawk
