#pragma once

#include <random>

#include "envoy/common/time.h"

#include "nighthawk/common/rate_limiter.h"

#include "external/envoy/source/common/common/logger.h"

#include "common/frequency.h"

#include "absl/random/random.h"
#include "absl/random/zipf_distribution.h"
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
  std::chrono::nanoseconds elapsed() override { return rate_limiter_->elapsed(); }

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

/**
 * A rate limiter which linearly ramps up to the desired frequency over the specified period.
 */
class RateLimiterBaseImpl : public RateLimiter {
public:
  RateLimiterBaseImpl(Envoy::TimeSource& time_source) : time_source_(time_source){};
  Envoy::TimeSource& timeSource() override { return time_source_; }
  absl::optional<Envoy::MonotonicTime> timeStarted() const override { return start_time_; }
  std::chrono::nanoseconds elapsed() override {
    // TODO(oschaaf): consider adding an explicit start() call to the interface.
    const auto now = time_source_.monotonicTime();
    if (start_time_ == absl::nullopt) {
      start_time_ = now;
    }
    return now - start_time_.value();
  }

private:
  Envoy::TimeSource& time_source_;
  absl::optional<Envoy::MonotonicTime> start_time_;
};

/**
 * Simple rate limiter that will allow acquiring at a linear pace.
 * The average rate is computed over a timeframe that starts at
 * the first call to tryAcquireOne().
 */
class LinearRateLimiter : public RateLimiterBaseImpl,
                          public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  LinearRateLimiter(Envoy::TimeSource& time_source, const Frequency frequency);
  bool tryAcquireOne() override;
  void releaseOne() override;

protected:
  int64_t acquireable_count_{0};
  uint64_t acquired_count_{0};
  const Frequency frequency_;
};

/**
 * A rate limiter which linearly ramps up to the desired frequency over the specified period.
 */
class LinearRampingRateLimiter : public RateLimiterBaseImpl,
                                 public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  LinearRampingRateLimiter(Envoy::TimeSource& time_source, const Frequency frequency);
  bool tryAcquireOne() override;
  void releaseOne() override;

private:
  int64_t acquireable_count_{0};
  uint64_t acquired_count_{0};
  const Frequency frequency_;
};

// We use an unsigned duration here to ensure only future points in time will be yielded.
// The consuming rate limiter will hold off opening up until the initial point in time plus the
// offset obtained via the delegate have transpired.
using RateLimiterDelegate = std::function<const std::chrono::duration<uint64_t, std::nano>()>;

using RateLimiterFilter = std::function<bool()>;

/**
 * Wraps a rate limiter, and allows plugging in a delegate which will be queried to offset the
 * timing of the underlying rate limiter.
 */
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

class UniformRandomDistributionSamplerImpl : public DiscreteNumericDistributionSampler {
public:
  UniformRandomDistributionSamplerImpl(const uint64_t upper_bound)
      : distribution_(0, upper_bound) {}
  uint64_t getValue() override { return distribution_(generator_); }
  uint64_t min() const override { return distribution_.min(); }
  uint64_t max() const override { return distribution_.max(); }

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

/**
 * Takes a probabilistic approach to suppress
 */
class GraduallyOpeningRateLimiterFilter : public FilteringRateLimiter {
public:
  GraduallyOpeningRateLimiterFilter(const std::chrono::nanoseconds ramp_time,
                                    DiscreteNumericDistributionSamplerPtr&& provider,
                                    RateLimiterPtr&& rate_limiter);

private:
  DiscreteNumericDistributionSamplerPtr provider_;
  const std::chrono::nanoseconds ramp_time_;
};

class ZipfRateLimiter : public FilteringRateLimiter {
public:
  ZipfRateLimiter(RateLimiterPtr&& rate_limiter);

private:
  absl::zipf_distribution<uint64_t> dist_;
  absl::InsecureBitGen g_;
};

} // namespace Nighthawk