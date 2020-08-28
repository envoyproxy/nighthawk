#include "test/common/fake_time_source.h"

namespace Nighthawk {

Envoy::SystemTime FakeIncrementingMonotonicTimeSource::systemTime() {
  Envoy::SystemTime epoch;
  return epoch;
}

Envoy::MonotonicTime FakeIncrementingMonotonicTimeSource::monotonicTime() {
  Envoy::MonotonicTime epoch;
  Envoy::MonotonicTime result = epoch + std::chrono::seconds(seconds_since_epoch_);
  ++seconds_since_epoch_;
  return result;
}

} // namespace Nighthawk
