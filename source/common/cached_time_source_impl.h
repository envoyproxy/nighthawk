#pragma once

#include <cstdint>

#include "external/envoy/envoy/common/time.h"
#include "external/envoy/envoy/event/dispatcher.h"

namespace Nighthawk {

/**
 * Time source which caches monotonic time. Intended for usage accross components to get a
 * consistent view of what "now" is during a single event loop cycle accross components. Minimizes
 * system calls to get the monotonic clock. updateApproximateMonotonicTime() must be explicitly
 * called on the associated dispatcher to update cached time.
 */
class CachedTimeSourceImpl : public Envoy::TimeSource {
public:
  /**
   * @param dispatcher Used to source/update cached monotonic time.
   */
  CachedTimeSourceImpl(Envoy::Event::Dispatcher& dispatcher) : dispatcher_(dispatcher) {}

  /**
   * @return Envoy::SystemTime current system time.
   */
  Envoy::SystemTime systemTime() override { return dispatcher_.timeSource().systemTime(); };

  /**
   * @return Envoy::MonotonicTime cached monotonic time.
   */
  Envoy::MonotonicTime monotonicTime() override { return dispatcher_.approximateMonotonicTime(); };

private:
  Envoy::Event::Dispatcher& dispatcher_;
};

} // namespace Nighthawk
