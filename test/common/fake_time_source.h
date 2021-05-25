#pragma once

#include "envoy/common/time.h"

namespace Nighthawk {
/**
 * Fake time source that ticks 1 second on every query, starting from the Unix epoch.
 */
class FakeIncrementingTimeSource : public Envoy::TimeSource {
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

  /**
   * Sets the current value of the system time.
   *
   * @param seconds the number of seconds to set the system time to.
   */
  void setSystemTimeSeconds(int seconds);
  /**
   * Sets the current value of the monotonic time.
   *
   * @param seconds the number of seconds to set the monotonic time to.
   */
  void setMonotonicTimeSeconds(int seconds);

private:
  int system_seconds_since_epoch_{0};
  int monotonic_seconds_since_epoch_{0};
};

} // namespace Nighthawk
