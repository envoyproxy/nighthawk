#pragma once

#include <random>

#include "envoy/common/time.h"

#include "nighthawk/common/rate_limiter.h"

#include "external/envoy/source/common/common/logger.h"

#include "common/frequency.h"

#include "absl/types/optional.h"

namespace Nighthawk {

/**
 * BurstingRatelimiter can be wrapped around another rate limiter. It has two modes:
 * 1. First it will be accumulating acquisitions by forwarding calls to the wrapped
 *    rate limiter, until the accumulated acquisitions equals the specified burst size.
 * 2. Release mode. In this mode, BatchingRateLimiter is in control and will be handling
 *    acquisition calls (returning true and substracting from the accumulated total until
 *    nothing is left, after which mode 1 will be entered again).
 */
class BurstingRateLimiter : public RateLimiter,
                            public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  BurstingRateLimiter(RateLimiterPtr&& rate_limiter, const uint64_t burst_size);
  bool tryAcquireOne() override;
  void releaseOne() override;

private:
  const RateLimiterPtr rate_limiter_;
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

private:
  Envoy::TimeSource& time_source_;
  int64_t acquireable_count_;
  uint64_t acquired_count_;
  const Frequency frequency_;
  bool started_{};
  Envoy::MonotonicTime started_at_;
};

using RandomDistributionGenerator =
    std::function<const std::chrono::duration<uint64_t, std::nano>()>;

// Wraps a rate limiter, and allows plugging in a delegate which will be queried to offset the
// timing of the underlying rate limiter.
class RandomDistributingRateLimiter : public RateLimiter,
                                      public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  RandomDistributingRateLimiter(Envoy::TimeSource& time_source, RateLimiterPtr&& rate_limiter,
                                RandomDistributionGenerator random_distribution_generator);
  bool tryAcquireOne() override;
  void releaseOne() override;

protected:
  const RandomDistributionGenerator random_distribution_generator_;

private:
  Envoy::TimeSource& time_source_;
  const RateLimiterPtr rate_limiter_;
  Envoy::MonotonicTime distributed_start_;
  bool distributed_start_set_{};
};

// Allows adding uniformly distributed random timing offsets to an underlying rate limiter.
class UniformDistributingRateLimiter : public RandomDistributingRateLimiter {
public:
  UniformDistributingRateLimiter(Envoy::TimeSource& time_source, RateLimiterPtr&& rate_limiter,
                                 const std::chrono::nanoseconds upper_bound);

private:
  std::default_random_engine generator_;
  std::uniform_int_distribution<uint64_t> distribution_;
};

} // namespace Nighthawk