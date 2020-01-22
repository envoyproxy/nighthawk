#include "common/phase_impl.h"

namespace Nighthawk {

absl::string_view PhaseImpl::id() const { return id_; }

Sequencer& PhaseImpl::sequencer() const { return *sequencer_; }

bool PhaseImpl::shouldMeasureLatencies() const { return should_measure_latencies_; }

void PhaseImpl::run() const {
  ENVOY_LOG(trace, "starting '{}' phase", id());
  sequencer().start();
  sequencer().waitForCompletion();
  ENVOY_LOG(trace, "finished '{}' phase", id());
}

} // namespace Nighthawk