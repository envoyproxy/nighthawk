#include "common/rate_limiter_impl.h"

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/common/assert.h"

namespace Nighthawk {

BurstingRateLimiter::BurstingRateLimiter(RateLimiterPtr&& rate_limiter, const uint64_t burst_size)
    : rate_limiter_(std::move(rate_limiter)), burst_size_(burst_size) {
  ASSERT(burst_size_ > 0);
}

bool BurstingRateLimiter::tryAcquireOne() {
  // Callers may not be able to actually make use of a successfull acquisition, and
  // call releaseOne() to indicate so. We remember state here so we can use that later when
  // that happens to restore it.
  previously_releasing_ = releasing_;

  if (releasing_) {
    // When releasing_ we drain the accumulated total until there's nothing left. We'll transit to
    // accumulating mode after that.
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
        // We have accumulated the burst size. Enter release mode and recurse.
        return tryAcquireOne();
      }
    }
  }

  previously_releasing_ = absl::nullopt;
  return false;
}

void BurstingRateLimiter::releaseOne() {
  ASSERT(accumulated_ < burst_size_);
  ASSERT(previously_releasing_ != absl::nullopt && previously_releasing_ == true);
  // The caller wasn't able to put its earlier successfull acquisition to good use, so we restore
  // state to what it was prior to that.
  accumulated_++;
  releasing_ = true; // release_ could only have been set earlier.
  previously_releasing_ = absl::nullopt;
}

LinearRateLimiter::LinearRateLimiter(Envoy::TimeSource& time_source, const Frequency frequency)
    : time_source_(time_source), acquireable_count_(0), acquired_count_(0), frequency_(frequency) {
  if (frequency.value() <= 0) {
    throw NighthawkException("Frequency must be > 0");
  }
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