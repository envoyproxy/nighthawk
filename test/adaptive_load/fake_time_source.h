#pragma once

#include "envoy/common/time.h"

namespace Nighthawk {
/**
 * Fake time source that ticks 1 second on every query, starting from the Unix epoch. Supports only
 * monotonicTime().
 */
class FakeIncrementingMonotonicTimeSource : public Envoy::TimeSource {
public:
  /**
   * Not supported.
   *
   * @return Envoy::SystemTime Fixed value of the Unix epoch.
   */
  Envoy::SystemTime systemTime() override;
  /**
   * Ticks forward 1 second on each call.
   *
   * @return Envoy::MonotonicTime Fake time value.
   */
  Envoy::MonotonicTime monotonicTime() override;

private:
  int unix_time_{0};
};

} // namespace Nighthawk
