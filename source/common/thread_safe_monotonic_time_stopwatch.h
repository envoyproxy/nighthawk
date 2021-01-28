#pragma once

#include "nighthawk/common/stopwatch.h"

#include "external/envoy/source/common/common/lock_guard.h"
#include "external/envoy/source/common/common/thread.h"

namespace Nighthawk {

/**
 * Utility class for thread safe tracking of elapsed monotonic time.
 * Example usage:
 *
 * ThreadSafeMontonicTimeStopwatch stopwatch;
 * int i = 0;
 * do {
 *   std::cerr << stopwatch.getElapsedNsAndReset() <<
 *    "ns elapsed since last iteration." << std::endl;
 * } while (++i < 100);
 */
class ThreadSafeMontonicTimeStopwatch : public Stopwatch {
public:
  /**
   * Construct a new ThreadSafe & MontonicTime-based Stopwatch object.
   */
  ThreadSafeMontonicTimeStopwatch() : start_(Envoy::MonotonicTime::min()) {}

  /**
   * @param time_source used to obtain a sample of the current monotonic time.
   * @return uint64_t 0 on the first invocation, and the number of elapsed nanoseconds since the
   * last invocation otherwise.
   */
  uint64_t getElapsedNsAndReset(Envoy::TimeSource& time_source) override;

private:
  Envoy::Thread::MutexBasicLockable lock_;
  Envoy::MonotonicTime start_ ABSL_GUARDED_BY(lock_);
};

} // namespace Nighthawk
