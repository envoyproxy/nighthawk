#pragma once

#include <memory>

#include "envoy/common/pure.h"

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
   * @return Envoy::TimeSource& time_source
   */
  virtual Envoy::TimeSource& timeSource() PURE;
  virtual absl::optional<Envoy::MonotonicTime> timeStarted() const PURE;
  virtual std::chrono::nanoseconds elapsed() const PURE;
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