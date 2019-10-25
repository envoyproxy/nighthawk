
#pragma once

#include "nighthawk/common/phase.h"
#include "nighthawk/common/rate_limiter.h"

namespace Nighthawk {

class PhaseImpl : public Phase {
public:
  PhaseImpl(absl::string_view id, SequencerPtr&& sequencer)
      : id_(std::string(id)), sequencer_(std::move(sequencer)) {}
  absl::string_view id() const override;
  Sequencer& sequencer() const override;

private:
  const std::string id_;
  const SequencerPtr sequencer_;
};

} // namespace Nighthawk