#pragma once

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
};

typedef std::unique_ptr<PlatformUtil> PlatformUtilPtr;

}