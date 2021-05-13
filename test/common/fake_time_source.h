#pragma once

#include "envoy/common/time.h"

namespace Nighthawk {
/**
 * Fake time source that ticks 1 second on every query, starting from the Unix epoch.
 */
class FakeIncrementingMonotonicTimeSource : public Envoy::TimeSource {
public:
  /**
   * Ticks forward 1 second on each call.
   *
   * @return Envoy::SystemTime Fake time value.
   */
  Envoy::SystemTime systemTime() override;
  /**
   * Ticks forward 1 second on each call.
   *
   * @return Envoy::MonotonicTime Fake time value.
   */
  Envoy::MonotonicTime monotonicTime() override;

private:
  int system_seconds_since_epoch_{0};
  int monotonic_seconds_since_epoch_{0};
};

} // namespace Nighthawk
