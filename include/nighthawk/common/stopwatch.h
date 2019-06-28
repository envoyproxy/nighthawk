#pragma once

#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "nighthawk/common/poolable.h"

namespace Nighthawk {

class Stopwatch {
public:
  virtual ~Stopwatch() = default;

  /**
   * Resets the stopwatch state.
   */
  virtual void reset() PURE;

  /**
   * Start measuring time
   */
  virtual void start() PURE;

  /**
   * Stop measuring time.
   */
  virtual void stop() PURE;

  /**
   * Return time elapsed since Start().
   */
  virtual std::chrono::nanoseconds elapsed() const PURE;
};

} // namespace Nighthawk
