
#pragma once

#include "envoy/common/time.h"

#include "nighthawk/common/phase.h"

#include "external/envoy/source/common/common/logger.h"

#include "absl/types/optional.h"

namespace Nighthawk {

class PhaseImpl : public Phase, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  /**
   * @param id Unique identifier of the pase (uniqueness not enforced).
   * @param sequencer Sequencer that will be used to execute this phase.
   * @param should_measure_latencies Indicates if latencies should be tracked for requests issued
   * during execution of this phase.
   * @param time_source Time source that will be used to query the clock.
   * @param start_time Optional starting time of the phase. Can be used to schedule phases ahead.
   */
  PhaseImpl(absl::string_view id, SequencerPtr&& sequencer, bool should_measure_latencies)
      : id_(std::string(id)), sequencer_(std::move(sequencer)),
        should_measure_latencies_(should_measure_latencies) {}
  absl::string_view id() const override;
  Sequencer& sequencer() const override;
  void run() const override;
  bool shouldMeasureLatencies() const override;

private:
  const std::string id_;
  const SequencerPtr sequencer_;
  const bool should_measure_latencies_;
};

} // namespace Nighthawk