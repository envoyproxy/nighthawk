#pragma once

#include "envoy/common/pure.h"
#include "envoy/common/time.h"

namespace Nighthawk {

class Stopwatch {
public:
  virtual ~Stopwatch() = default;
  /**
   * @param time_source used to obtain a sample of the current time.
   * @return uint64_t 0 on the first invocation, and the number of elapsed nanoseconds since the
   * last invocation otherwise.
   */
  virtual uint64_t getElapsedNsAndReset(Envoy::TimeSource& time_source) PURE;
};

} // namespace Nighthawk