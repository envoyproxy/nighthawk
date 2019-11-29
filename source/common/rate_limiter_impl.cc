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

RampingLinearRateLimiter::RampingLinearRateLimiter(Envoy::TimeSource& time_source,
                                                   const std::chrono::nanoseconds ramp_time,
                                                   const Frequency frequency)
    : LinearRateLimiter(time_source, frequency), final_frequency_(frequency),
      ramp_time_(ramp_time) {
  if (ramp_time.count() <= 0) {
    throw NighthawkException("ramp_time must be > 0");
  }
}

bool RampingLinearRateLimiter::tryAcquireOne() {
  bool return_value = false;
  if (!started_ || (frequency_.value() != final_frequency_.value())) {
    const auto elapsed_since_start = time_source_.monotonicTime() - started_at_;
    const double fraction =
        1.0 - ((ramp_time_.count() - elapsed_since_start.count()) / (ramp_time_.count() * 1.0));
    frequency_ = Frequency(std::round(final_frequency_.value() * fraction));
    // LinearRateLimiter tracks how many ought to have been acquired and will compensate when we
    // change the frequency. We're greedy here to disable that corrective behaviour when ramping.
    while (LinearRateLimiter::tryAcquireOne()) {
      return_value = true;
    }
  } else {
    return_value = LinearRateLimiter::tryAcquireOne();
  }
  return return_value;
}

DelegatingRateLimiter::DelegatingRateLimiter(Envoy::TimeSource& time_source,
                                             RateLimiterPtr&& rate_limiter,
                                             RateLimiterDelegate random_distribution_generator)
    : random_distribution_generator_(std::move(random_distribution_generator)),
      time_source_(time_source), rate_limiter_(std::move(rate_limiter)) {}

bool DelegatingRateLimiter::tryAcquireOne() {
  if (distributed_start_ == absl::nullopt) {
    if (rate_limiter_->tryAcquireOne()) {
      distributed_start_ = time_source_.monotonicTime() + random_distribution_generator_();
    }
  }

  if (distributed_start_ != absl::nullopt && distributed_start_ <= time_source_.monotonicTime()) {
    distributed_start_ = absl::nullopt;
    return true;
  }

  return false;
}

void DelegatingRateLimiter::releaseOne() {
  distributed_start_ = absl::nullopt;
  rate_limiter_->releaseOne();
}

DistributionSamplingRateLimiterImpl::DistributionSamplingRateLimiterImpl(
    Envoy::TimeSource& time_source, DiscreteNumericDistributionSamplerPtr&& provider,
    RateLimiterPtr&& rate_limiter)
    : DelegatingRateLimiter(
          time_source, std::move(rate_limiter),
          [this]() { return std::chrono::duration<uint64_t, std::nano>(provider_->getValue()); }),
      provider_(std::move(provider)) {}

} // namespace Nighthawk