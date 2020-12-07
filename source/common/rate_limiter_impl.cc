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

ScheduledStartingRateLimiter::ScheduledStartingRateLimiter(
    RateLimiterPtr&& rate_limiter, const Envoy::MonotonicTime scheduled_starting_time)
    : ForwardingRateLimiterImpl(std::move(rate_limiter)),
      scheduled_starting_time_(scheduled_starting_time) {
  if (timeSource().monotonicTime() >= scheduled_starting_time_) {
    ENVOY_LOG(error, "Scheduled starting time exceeded. This may cause unintended bursty traffic.");
  }
}

bool ScheduledStartingRateLimiter::tryAcquireOne() {
  if (timeSource().monotonicTime() < scheduled_starting_time_) {
    aquisition_attempted_ = true;
    return false;
  }
  // If we start forwarding right away on the first attempt that is remarkable, so leave a hint
  // about this happening in the logs.
  if (!aquisition_attempted_) {
    aquisition_attempted_ = true;
    ENVOY_LOG(warn, "ScheduledStartingRateLimiter: first acquisition attempt was late");
  }
  return rate_limiter_->tryAcquireOne();
}

void ScheduledStartingRateLimiter::releaseOne() {
  if (timeSource().monotonicTime() < scheduled_starting_time_) {
    throw NighthawkException("Unexpected call to releaseOne()");
  }
  return rate_limiter_->releaseOne();
}

LinearRateLimiter::LinearRateLimiter(Envoy::TimeSource& time_source, const Frequency frequency)
    : RateLimiterBaseImpl(time_source), frequency_(frequency) {
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

  // As the common case for configured execution duration is in seconds, we shift phase so that
  // acquisitions timed at one second boundaries will be avoided.
  // For example, at three rps our timings should look like [0.16667s, 0.5s, 0.83333s,...].
  // This helps produce more intuitive/steady counter values.
  const auto phase_shifted = elapsed() + (frequency_.interval() / 2);
  acquireable_count_ =
      static_cast<int64_t>(std::floor(phase_shifted / frequency_.interval())) - acquired_count_;
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
  const std::chrono::nanoseconds elapsed_time = elapsed();
  double elapsed_fraction = 1.0;
  if (elapsed_time < ramp_time_) {
    elapsed_fraction -= static_cast<double>(ramp_time_.count() - elapsed_time.count()) /
                        static_cast<double>(ramp_time_.count());
  }
  const double current_frequency = elapsed_fraction * frequency_.value();
  // If we'd be at a constant pace, we can expect elapsed seconds * frequency requests.
  // However, as we are linearly ramping, we can expect half of that, hence we
  // divide by two.
  const int64_t total = std::round((elapsed_time.count() / 1e9) * current_frequency / 2.0);
  acquireable_count_ = total - acquired_count_;
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
  const Envoy::MonotonicTime now = timeSource().monotonicTime();
  if (rate_limiter_->tryAcquireOne()) {
    const Envoy::MonotonicTime adjusted = now + random_distribution_generator_();
    // We track a sorted list of timings, where the one at the front is the one that should
    // be applied the soonest.
    distributed_timings_.insert(
        std::upper_bound(distributed_timings_.begin(), distributed_timings_.end(), adjusted),
        adjusted);
  }

  if (!distributed_timings_.empty() && distributed_timings_.front() <= now) {
    distributed_timings_.pop_front();
    sanity_check_pending_release_ = false;
    return true;
  }

  return false;
}

void DelegatingRateLimiterImpl::releaseOne() {
  RELEASE_ASSERT(!sanity_check_pending_release_,
                 "unexpected call to DelegatingRateLimiterImpl::releaseOne()");
  sanity_check_pending_release_ = true;
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
            const auto elapsed_time = elapsed();
            if (elapsed_time < ramp_time_) {
              // We want to linearly increase the probability of returning true
              // below. We can derive that from the elapsed fraction of ramp_time.
              const double probability =
                  1.0 - static_cast<double>(ramp_time_.count() - elapsed_time.count()) /
                            (ramp_time_.count() * 1.0);
              // Get a random number r, where 0 < r ≤ 1.
              const double random_between_0_and_1 = 1.0 * provider_->getValue() / provider_->max();
              // Given a uniform distribution, the fraction of the ramp
              // will translate into the probability of opening up we are looking for.
              return random_between_0_and_1 < probability;
            }
            // Ramping is complete, and as such this filter has completely opened up.
            return true;
          }),
      provider_(std::move(provider)), ramp_time_(ramp_time) {
  if (ramp_time <= 0ns) {
    throw NighthawkException("ramp_time must be positive and > 0ns");
  }
  if (provider_->min() != 1) {
    throw NighthawkException("min value of the distribution provider must equal 1");
  }
  if (provider_->max() != 1000000) {
    throw NighthawkException("max value of the distribution provider must equal 1000000");
  }
}

ZipfRateLimiterImpl::ZipfRateLimiterImpl(RateLimiterPtr&& rate_limiter, double q, double v,
                                         ZipfBehavior behavior)
    : FilteringRateLimiterImpl(std::move(rate_limiter),
                               [this]() {
                                 return behavior_ == ZipfBehavior::ZIPF_PSEUDO_RANDOM ? dist_(mt_)
                                                                                      : dist_(g_);
                               }),
      behavior_(behavior) {
  if (v <= 0) {
    throw NighthawkException("v should be > 0");
  }
  if (q <= 1) {
    throw NighthawkException("q should be > 1");
  }
  dist_ = absl::zipf_distribution<uint64_t>(1, q, v);
}

} // namespace Nighthawk