#include "common/phase_impl.h"

namespace Nighthawk {

absl::string_view PhaseImpl::id() const { return id_; }

Sequencer& PhaseImpl::sequencer() const { return *sequencer_; }

} // namespace Nighthawk