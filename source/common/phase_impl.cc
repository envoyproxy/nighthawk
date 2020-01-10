#include "common/phase_impl.h"

namespace Nighthawk {

absl::string_view PhaseImpl::id() const { return id_; }

Sequencer& PhaseImpl::sequencer() const { return *sequencer_; }

bool PhaseImpl::shouldMeasureLatencies() const { return measure_latencies_; }

void PhaseImpl::run() const {
  ENVOY_LOG(trace, "starting '{}' phase", id_);
  // If applicable, spin until it is time to start.
  while (start_time_.has_value() && start_time_ > time_source_.monotonicTime()) {
    ;
  }
  sequencer().start();
  sequencer().waitForCompletion();
  ENVOY_LOG(trace, "finished '{}' phase", id_);
}

} // namespace Nighthawk