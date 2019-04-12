#include "nighthawk/source/common/rate_limiter_impl.h"

#include "common/common/assert.h"

#include "nighthawk/common/exception.h"

namespace Nighthawk {

LinearRateLimiter::LinearRateLimiter(Envoy::TimeSource& time_source, const Frequency frequency)
    : time_source_(time_source), acquireable_count_(0), acquired_count_(0), frequency_(frequency) {
  ASSERT(frequency.value() > 0, "Frequency must be > 0");
}

bool LinearRateLimiter::tryAcquireOne() {
  // TODO(oschaaf): consider adding an explicit start() call to the interface.
  if (!started_) {
    started_at_ = time_source_.monotonicTime();
    started_ = true;
  }
  if (acquireable_count_ > 0) {
    acquireable_count_--;
    acquired_count_++;
    return true;
  }

  const auto elapsed_since_start = time_source_.monotonicTime() - started_at_;
  acquireable_count_ = (elapsed_since_start / frequency_.interval()) - acquired_count_;
  return acquireable_count_ > 0 ? tryAcquireOne() : false;
}

void LinearRateLimiter::releaseOne() {
  acquireable_count_++;
  acquired_count_--;
}

} // namespace Nighthawk