#pragma once

#include <random>

#include "envoy/common/time.h"

#include "nighthawk/common/rate_limiter.h"

#include "external/envoy/source/common/common/logger.h"

#include "common/frequency.h"

#include "absl/types/optional.h"

namespace Nighthawk {

class ForwardingRateLimiterImpl : public RateLimiter {
public:
  ForwardingRateLimiterImpl(RateLimiterPtr&& rate_limiter)
      : rate_limiter_(std::move(rate_limiter)) {}
  Envoy::TimeSource& timeSource() override { return rate_limiter_->timeSource(); }
  absl::optional<Envoy::MonotonicTime> timeStarted() const override {
    return rate_limiter_->timeStarted();
  }
  std::chrono::nanoseconds elapsed() const override { return rate_limiter_->elapsed(); }

protected:
  const RateLimiterPtr rate_limiter_;
};

/**
 * BurstingRatelimiter can be wrapped around another rate limiter. It has two modes:
 * 1. First it will be accumulating acquisitions by forwarding calls to the wrapped
 *    rate limiter, until the accumulated acquisitions equals the specified burst size.
 * 2. Release mode. In this mode, BatchingRateLimiter is in control and will be handling
 *    acquisition calls (returning true and substracting from the accumulated total until
 *    nothing is left, after which mode 1 will be entered again).
 */
class BurstingRateLimiter : public ForwardingRateLimiterImpl,
                            public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  BurstingRateLimiter(RateLimiterPtr&& rate_limiter, const uint64_t burst_size);
  bool tryAcquireOne() override;
  void releaseOne() override;

private:
  const uint64_t burst_size_;
  uint64_t accumulated_{0};
  bool releasing_{};
  absl::optional<bool> previously_releasing_; // Solely used for sanity checking.
};

// Simple rate limiter that will allow acquiring at a linear pace.
// The average rate is computed over a timeframe that starts at
// the first call to tryAcquireOne().
class LinearRateLimiter : public RateLimiter,
                          public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  LinearRateLimiter(Envoy::TimeSource& time_source, const Frequency frequency);
  bool tryAcquireOne() override;
  void releaseOne() override;
  Envoy::TimeSource& timeSource() override { return time_source_; }
  absl::optional<Envoy::MonotonicTime> timeStarted() const override { return start_time_; }
  std::chrono::nanoseconds elapsed() const override {
    const auto now = time_source_.monotonicTime();
    return now - start_time_.value_or(now);
  }

protected:
  Envoy::TimeSource& time_source_;
  int64_t acquireable_count_{0};
  uint64_t acquired_count_{0};
  Frequency frequency_;
  absl::optional<Envoy::MonotonicTime> start_time_;
};

// We use an unsigned duration here to ensure only future points in time will be yielded.
// The consuming rate limiter will hold off opening up until the initial point in time plus the
// offset obtained via the delegate have transpired.
using RateLimiterDelegate = std::function<const std::chrono::duration<uint64_t, std::nano>()>;

using RateLimiterFilter = std::function<bool()>;

// Wraps a rate limiter, and allows plugging in a delegate which will be queried to offset the
// timing of the underlying rate limiter.
class DelegatingRateLimiter : public ForwardingRateLimiterImpl,
                              public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  DelegatingRateLimiter(RateLimiterPtr&& rate_limiter,
                        RateLimiterDelegate random_distribution_generator);
  bool tryAcquireOne() override;
  void releaseOne() override;

protected:
  const RateLimiterDelegate random_distribution_generator_;

private:
  absl::optional<Envoy::MonotonicTime> distributed_start_;
};

/**
 * A rate limiter which linearly ramps up to the desired frequency over the specified period.
 */
class LinearRampingRateLimiter : public LinearRateLimiter {
public:
  LinearRampingRateLimiter(Envoy::TimeSource& time_source, const std::chrono::nanoseconds ramp_time,
                           const Frequency frequency);
  bool tryAcquireOne() override;

private:
  const Frequency final_frequency_;
  const std::chrono::nanoseconds ramp_time_;
};

class UniformRandomDistributionSamplerImpl : public DiscreteNumericDistributionSampler {
public:
  UniformRandomDistributionSamplerImpl(const std::chrono::duration<uint64_t, std::nano> upper_bound)
      : distribution_(0, upper_bound.count()) {}
  uint64_t getValue() override { return distribution_(generator_); }

private:
  std::default_random_engine generator_;
  std::uniform_int_distribution<uint64_t> distribution_;
};

// Allows adding uniformly distributed random timing offsets to an underlying rate limiter.
class DistributionSamplingRateLimiterImpl : public DelegatingRateLimiter {
public:
  DistributionSamplingRateLimiterImpl(DiscreteNumericDistributionSamplerPtr&& provider,
                                      RateLimiterPtr&& rate_limiter);

private:
  DiscreteNumericDistributionSamplerPtr provider_;
};

// Wraps a rate limiter, and allows plugging in a delegate which will be queried to apply a
// filter to acquisitions.
class FilteringRateLimiter : public ForwardingRateLimiterImpl,
                             public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  FilteringRateLimiter(RateLimiterPtr&& rate_limiter, RateLimiterFilter filter);
  bool tryAcquireOne() override;
  void releaseOne() override { rate_limiter_->releaseOne(); }

protected:
  const RateLimiterFilter filter_;
};

class LinearlyOpeningRateLimiterFilter : public FilteringRateLimiter {
public:
  LinearlyOpeningRateLimiterFilter(const std::chrono::nanoseconds ramp_time,
                                   RateLimiterPtr&& rate_limiter);

private:
  UniformRandomDistributionSamplerImpl sampler_;
  const std::chrono::nanoseconds ramp_time_;
};

} // namespace Nighthawk