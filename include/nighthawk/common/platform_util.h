#pragma once

#include <chrono>
#include <memory>

#include "envoy/common/pure.h"

namespace Nighthawk {

/**
 * A place to store platform specific utilities.
 */
class PlatformUtil {
public:
  virtual ~PlatformUtil() = default;

  /**
   * Yields the current thread. The OS decides which one to run.
   */
  virtual void yieldCurrentThread() const PURE;
  /**
   * @param duration duration that the calling thread should sleep.
   */
  virtual void sleep(std::chrono::microseconds duration) const PURE;
};

using PlatformUtilPtr = std::unique_ptr<PlatformUtil>;

} // namespace Nighthawk