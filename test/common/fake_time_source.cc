#include "test/common/fake_time_source.h"

namespace Nighthawk {

Envoy::SystemTime FakeIncrementingMonotonicTimeSource::systemTime() {
  Envoy::SystemTime epoch;
  Envoy::SystemTime result = epoch + std::chrono::seconds(system_seconds_since_epoch_);
  ++system_seconds_since_epoch_;
  return result;
}

Envoy::MonotonicTime FakeIncrementingMonotonicTimeSource::monotonicTime() {
  Envoy::MonotonicTime epoch;
  Envoy::MonotonicTime result = epoch + std::chrono::seconds(monotonic_seconds_since_epoch_);
  ++monotonic_seconds_since_epoch_;
  return result;
}

} // namespace Nighthawk
