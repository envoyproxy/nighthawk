#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "external/envoy/test/test_common/simulated_time_system.h"
#include "external/envoy/test/test_common/utility.h"

#include "source/common/thread_safe_monotonic_time_stopwatch.h"

#include "test/common/fake_time_source.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using namespace std::chrono_literals;

class SimTimeStopwatchTest : public testing::Test, public Envoy::Event::TestUsingSimulatedTime {};

TEST_F(SimTimeStopwatchTest, TestElapsedAndReset) {
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

TEST(ThreadSafeStopwatchTest, ThreadedStopwatchSpamming) {
  constexpr uint64_t kFakeTimeSourceDefaultTick = 1000000000;
  constexpr uint32_t kNumThreads = 100;
  ThreadSafeMontonicTimeStopwatch stopwatch;
  FakeIncrementingTimeSource time_system;
  std::vector<std::thread> threads(kNumThreads);
  std::promise<void> signal_all_threads_running;
  std::shared_future<void> future(signal_all_threads_running.get_future());

  // The first call should always return 0.
  EXPECT_EQ(stopwatch.getElapsedNsAndReset(time_system), 0);
  for (std::thread& thread : threads) {
    thread = std::thread([&stopwatch, &time_system, kFakeTimeSourceDefaultTick, future] {
      // We wait for all threads to be up and running here to maximize concurrency
      // of the call below.
      future.wait();
      // Subsequent calls should always return 1s.
      EXPECT_EQ(stopwatch.getElapsedNsAndReset(time_system), kFakeTimeSourceDefaultTick);
    });
  }
  signal_all_threads_running.set_value();
  for (std::thread& thread : threads) {
    thread.join();
  }
  // Verify monotonic time has advanced right up to the point we expect
  // it to, based on the number of threads that have excecuted.
  EXPECT_EQ(time_system.monotonicTime().time_since_epoch().count(),
            (kNumThreads * kFakeTimeSourceDefaultTick) + kFakeTimeSourceDefaultTick);
}

} // namespace
} // namespace Nighthawk
