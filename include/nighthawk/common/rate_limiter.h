#pragma once

#include <chrono>
#include <memory>

#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "absl/types/optional.h"

namespace Nighthawk {

/**
 * Abstract rate limiter interface.
 */
class RateLimiter {
public:
  virtual ~RateLimiter() = default;

  /**
   * Acquire a controlled resource.
   * @return true Indicates success.
   * @return false Indicates failure to acquire.
   */
  virtual bool tryAcquireOne() PURE;

  /**
   * Releases a controlled resource.
   */
  virtual void releaseOne() PURE;

  /**
   * @return Envoy::TimeSource& time_source used to track time.
   */
  virtual Envoy::TimeSource& timeSource() PURE;
  /**
   * @return std::chrono::nanoseconds elapsed since the first call to tryAcquireOne(). Used by some
   * rate limiter implementations to compute acquisition rate.
   */
  virtual std::chrono::nanoseconds elapsed() PURE;
};

using RateLimiterPtr = std::unique_ptr<RateLimiter>;

/**
 * Interface to sample discrete numeric distributions.
 */
class DiscreteNumericDistributionSampler {
public:
  virtual ~DiscreteNumericDistributionSampler() = default;
  virtual uint64_t getValue() PURE;
};

using DiscreteNumericDistributionSamplerPtr = std::unique_ptr<DiscreteNumericDistributionSampler>;

} // namespace Nighthawk