#include "common/phase_impl.h"

namespace Nighthawk {

absl::string_view PhaseImpl::id() const { return id_; }

Sequencer& PhaseImpl::sequencer() const { return *sequencer_; }

void PhaseImpl::run() const {
  ENVOY_LOG(trace, "starting '{}' phase", id_);
  sequencer().start();
  sequencer().waitForCompletion();
  ENVOY_LOG(trace, "finished '{}' phase", id_);
}

} // namespace Nighthawk