#include "test/adaptive_load/fake_time_source.h"

namespace Nighthawk {

Envoy::SystemTime FakeIncrementingMonotonicTimeSource::systemTime() {
  Envoy::SystemTime epoch;
  return epoch;
}

Envoy::MonotonicTime FakeIncrementingMonotonicTimeSource::monotonicTime() {
  ++seconds_since_epoch_;
  Envoy::MonotonicTime epoch;
  return epoch + std::chrono::seconds(seconds_since_epoch_);
}

} // namespace Nighthawk
