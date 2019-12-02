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
    throw NighthawkException("Frequency must be > 0");
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

LinearRampingRateLimiter::LinearRampingRateLimiter(Envoy::TimeSource& time_source,
                                                   const Frequency frequency)
    : RateLimiterBaseImpl(time_source), frequency_(frequency) {
  if (frequency_.value() <= 0) {
    throw NighthawkException("frequency must be > 0");
  }
}

bool LinearRampingRateLimiter::tryAcquireOne() {
  if (acquireable_count_) {
    acquired_count_++;
    return acquireable_count_--;
  }
  const auto elapsed_time = elapsed();
  const std::chrono::duration<double> chrono_seconds(elapsed_time);
  const double seconds = chrono_seconds.count();
  const double frequency = seconds * frequency_.value();
  const uint64_t total = std::round(seconds * frequency / 2.0);
  acquireable_count_ = total - acquired_count_;
  return acquireable_count_ > 0 ? tryAcquireOne() : false;
}

void LinearRampingRateLimiter::releaseOne() {
  acquireable_count_++;
  acquired_count_--;
}

DelegatingRateLimiter::DelegatingRateLimiter(RateLimiterPtr&& rate_limiter,
                                             RateLimiterDelegate random_distribution_generator)
    : ForwardingRateLimiterImpl(std::move(rate_limiter)),
      random_distribution_generator_(std::move(random_distribution_generator)) {}

bool DelegatingRateLimiter::tryAcquireOne() {
  const auto now = timeSource().monotonicTime();
  if (distributed_start_ == absl::nullopt) {
    if (rate_limiter_->tryAcquireOne()) {
      distributed_start_ = now + random_distribution_generator_();
    }
  }

  if (distributed_start_ != absl::nullopt && distributed_start_ <= now) {
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
    DiscreteNumericDistributionSamplerPtr&& provider, RateLimiterPtr&& rate_limiter)
    : DelegatingRateLimiter(
          std::move(rate_limiter),
          [this]() { return std::chrono::duration<uint64_t, std::nano>(provider_->getValue()); }),
      provider_(std::move(provider)) {}

FilteringRateLimiter::FilteringRateLimiter(RateLimiterPtr&& rate_limiter, RateLimiterFilter filter)
    : ForwardingRateLimiterImpl(std::move(rate_limiter)), filter_(std::move(filter)) {}

bool FilteringRateLimiter::tryAcquireOne() {
  return rate_limiter_->tryAcquireOne() ? filter_() : false;
}

GraduallyOpeningRateLimiterFilter::GraduallyOpeningRateLimiterFilter(
    const std::chrono::nanoseconds ramp_time, DiscreteNumericDistributionSamplerPtr&& provider,
    RateLimiterPtr&& rate_limiter)
    : FilteringRateLimiter(
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
      provider_(std::move(provider)), ramp_time_(ramp_time) {
  RELEASE_ASSERT(provider_->min() == 1 && provider_->max() == 1000000,
                 "expected a distribution ranging from 1-1000000");
}

ZipfRateLimiter::ZipfRateLimiter(RateLimiterPtr&& rate_limiter)
    : FilteringRateLimiter(std::move(rate_limiter), [this]() { return dist_(g_); }), dist_(1) {}

} // namespace Nighthawk