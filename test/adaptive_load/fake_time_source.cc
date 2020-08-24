#include "test/adaptive_load/fake_time_source.h"

namespace Nighthawk {

Envoy::SystemTime FakeIncrementingMonotonicTimeSource::systemTime() {
  Envoy::SystemTime epoch;
  return epoch;
}

Envoy::MonotonicTime FakeIncrementingMonotonicTimeSource::monotonicTime() {
  ++unix_time_;
  Envoy::MonotonicTime epoch;
  return epoch + std::chrono::seconds(unix_time_);
}

} // namespace Nighthawk
