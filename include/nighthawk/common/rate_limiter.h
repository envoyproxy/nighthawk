#pragma once

#include <memory>

#include "envoy/common/pure.h"

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
};

typedef std::unique_ptr<RateLimiter> RateLimiterPtr;

} // namespace Nighthawk