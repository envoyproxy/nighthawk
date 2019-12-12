
#pragma once

#include "nighthawk/common/phase.h"

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {

class PhaseImpl : public Phase, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  PhaseImpl(absl::string_view id, SequencerPtr&& sequencer, bool measure_latencies)
      : id_(std::string(id)), sequencer_(std::move(sequencer)),
        measure_latencies_(measure_latencies) {}
  absl::string_view id() const override;
  Sequencer& sequencer() const override;
  void run() const override;
  virtual bool measureLatencies() const override;

private:
  const std::string id_;
  const SequencerPtr sequencer_;
  const bool measure_latencies_;
};

} // namespace Nighthawk