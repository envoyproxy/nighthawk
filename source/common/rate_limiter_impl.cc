#include "common/rate_limiter_impl.h"

#include "common/common/assert.h"

#include "nighthawk/common/exception.h"

namespace Nighthawk {

BurstingRateLimiter::BurstingRateLimiter(RateLimiterPtr&& rate_limiter, const uint64_t burst_size)
    : rate_limiter_(std::move(rate_limiter)), burst_size_(burst_size) {
  ASSERT(burst_size_ > 0);
}

bool BurstingRateLimiter::tryAcquireOne() {
  if (releasing_) {
    accumulated_--;
    releasing_ = accumulated_ > 0;
    return true;
  } else {
    // We are greedy here, which should help with timely releases of bursts when
    // the system is lagging behind.
    while (rate_limiter_->tryAcquireOne()) {
      accumulated_++;
      if ((accumulated_ % burst_size_) == 0) {
        releasing_ = true;
        return tryAcquireOne();
      }
    }
  }
  return false;
}

void BurstingRateLimiter::releaseOne() {
  if (releasing_) {
    accumulated_++;
    ASSERT(accumulated_ <= burst_size_);
  } else {
    rate_limiter_->releaseOne();
  }
}

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
  acquireable_count_ =
      static_cast<int64_t>(std::floor(elapsed_since_start / frequency_.interval())) -
      acquired_count_;
  return acquireable_count_ > 0 ? tryAcquireOne() : false;
}

void LinearRateLimiter::releaseOne() {
  acquireable_count_++;
  acquired_count_--;
}

} // namespace Nighthawk