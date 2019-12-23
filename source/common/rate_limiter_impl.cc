#include "common/rate_limiter_impl.h"

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/common/assert.h"

namespace Nighthawk {

using namespace std::chrono_literals;

BurstingRateLimiter::BurstingRateLimiter(RateLimiterPtr&& rate_limiter, const uint64_t burst_size)
    : ForwardingRateLimiterImpl(std::move(rate_limiter)), burst_size_(burst_size) {
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
    : RateLimiterBaseImpl(time_source), acquireable_count_(0), acquired_count_(0),
      frequency_(frequency) {
  if (frequency.value() <= 0) {
    throw NighthawkException(fmt::format("frequency must be <= 0, value: {}", frequency.value()));
  }
}

bool LinearRateLimiter::tryAcquireOne() {
  // TODO(oschaaf): consider adding an explicit start() call to the interface.
  if (acquireable_count_ > 0) {
    acquireable_count_--;
    acquired_count_++;
    return true;
  }

  acquireable_count_ =
      static_cast<int64_t>(std::floor(elapsed() / frequency_.interval())) - acquired_count_;
  return acquireable_count_ > 0 ? tryAcquireOne() : false;
}

void LinearRateLimiter::releaseOne() {
  acquireable_count_++;
  acquired_count_--;
}

LinearRampingRateLimiterImpl::LinearRampingRateLimiterImpl(Envoy::TimeSource& time_source,
                                                           const std::chrono::nanoseconds ramp_time,
                                                           const Frequency frequency)
    : RateLimiterBaseImpl(time_source), ramp_time_(ramp_time), frequency_(frequency) {
  if (frequency_.value() <= 0) {
    throw NighthawkException(fmt::format("frequency must be > 0, value: {}", frequency.value()));
  }
  if (ramp_time <= 0ns) {
    throw NighthawkException(
        fmt::format("ramp_time must be positive, value: {}", ramp_time.count()));
  }
}

bool LinearRampingRateLimiterImpl::tryAcquireOne() {
  if (acquireable_count_) {
    acquired_count_++;
    return acquireable_count_--;
  }

  const std::chrono::nanoseconds elapsed_time = elapsed() + 1ns;
  double elapsed_fraction = 1.0;
  if (elapsed_time < ramp_time_) {
    elapsed_fraction -= static_cast<double>(ramp_time_.count() - elapsed_time.count()) /
                        static_cast<double>(ramp_time_.count());
  }

  const double current_frequency = elapsed_fraction * (frequency_.value() * 1.0);
  // If we'd be at a constant pace, we can expect duration * frequency requests.
  // However, as we are linearly ramping, we can expect half of that, hence we
  // divide by two.
  const double chrono_seconds =
      std::chrono::duration_cast<std::chrono::duration<double>>(elapsed_time).count();
  const double total = chrono_seconds * current_frequency / 2.0;
  acquireable_count_ = std::round(total) - acquired_count_;
  return acquireable_count_ > 0 ? tryAcquireOne() : false;
}

void LinearRampingRateLimiterImpl::releaseOne() {
  acquireable_count_++;
  acquired_count_--;
}

DelegatingRateLimiterImpl::DelegatingRateLimiterImpl(
    RateLimiterPtr&& rate_limiter, RateLimiterDelegate random_distribution_generator)
    : ForwardingRateLimiterImpl(std::move(rate_limiter)),
      random_distribution_generator_(std::move(random_distribution_generator)) {}

bool DelegatingRateLimiterImpl::tryAcquireOne() {
  if (distributed_start_ == absl::nullopt) {
    if (rate_limiter_->tryAcquireOne()) {
      distributed_start_ = timeSource().monotonicTime() + random_distribution_generator_();
    }
  }

  if (distributed_start_ != absl::nullopt && distributed_start_ <= timeSource().monotonicTime()) {
    distributed_start_ = absl::nullopt;
    return true;
  }

  return false;
}

void DelegatingRateLimiterImpl::releaseOne() {
  distributed_start_ = absl::nullopt;
  rate_limiter_->releaseOne();
}

DistributionSamplingRateLimiterImpl::DistributionSamplingRateLimiterImpl(
    DiscreteNumericDistributionSamplerPtr&& provider, RateLimiterPtr&& rate_limiter)
    : DelegatingRateLimiterImpl(
          std::move(rate_limiter),
          [this]() { return std::chrono::duration<uint64_t, std::nano>(provider_->getValue()); }),
      provider_(std::move(provider)) {}

FilteringRateLimiterImpl::FilteringRateLimiterImpl(RateLimiterPtr&& rate_limiter,
                                                   RateLimiterFilter filter)
    : ForwardingRateLimiterImpl(std::move(rate_limiter)), filter_(std::move(filter)) {}

bool FilteringRateLimiterImpl::tryAcquireOne() {
  return rate_limiter_->tryAcquireOne() ? filter_() : false;
}

GraduallyOpeningRateLimiterFilter::GraduallyOpeningRateLimiterFilter(
    const std::chrono::nanoseconds ramp_time, DiscreteNumericDistributionSamplerPtr&& provider,
    RateLimiterPtr&& rate_limiter)
    : FilteringRateLimiterImpl(
          std::move(rate_limiter),
          [this]() {
            if (elapsed() < ramp_time_) {
              const double chance_percentage =
                  100.0 - (static_cast<double>(ramp_time_.count() - elapsed().count()) /
                           (ramp_time_.count() * 1.0)) *
                              100.0;
              return std::round(provider_->getValue() / 10000.0) <= chance_percentage;
            }
            return true;
          }),
      provider_(std::move(provider)), ramp_time_(ramp_time) {}

} // namespace Nighthawk