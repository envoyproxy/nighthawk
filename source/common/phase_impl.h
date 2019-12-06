
#pragma once

#include "nighthawk/common/phase.h"

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {

class PhaseImpl : public Phase, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  PhaseImpl(absl::string_view id, SequencerPtr&& sequencer)
      : id_(std::string(id)), sequencer_(std::move(sequencer)) {}
  absl::string_view id() const override;
  Sequencer& sequencer() const override;
  void run() const override;

private:
  const std::string id_;
  const SequencerPtr sequencer_;
};

} // namespace Nighthawk