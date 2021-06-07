#include "source/common/thread_safe_monotonic_time_stopwatch.h"

namespace Nighthawk {

uint64_t ThreadSafeMontonicTimeStopwatch::getElapsedNsAndReset(Envoy::TimeSource& time_source) {
  Envoy::Thread::LockGuard guard(lock_);
  // Note that we obtain monotonic time under lock, to ensure that start_ will be updated
  // monotonically.
  const Envoy::MonotonicTime new_time = time_source.monotonicTime();
  const uint64_t elapsed_ns =
      start_ == Envoy::MonotonicTime::min() ? 0 : (new_time - start_).count();
  start_ = new_time;
  return elapsed_ns;
}

} // namespace Nighthawk