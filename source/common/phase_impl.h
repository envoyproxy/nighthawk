
#pragma once

#include "envoy/common/time.h"

#include "nighthawk/common/phase.h"

#include "external/envoy/source/common/common/logger.h"

#include "absl/types/optional.h"

namespace Nighthawk {

class PhaseImpl : public Phase, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  PhaseImpl(absl::string_view id, SequencerPtr&& sequencer, bool measure_latencies,
            Envoy::TimeSource& time_source, absl::optional<Envoy::MonotonicTime> start_time)
      : id_(std::string(id)), sequencer_(std::move(sequencer)),
        measure_latencies_(measure_latencies), time_source_(time_source), start_time_(start_time) {}
  absl::string_view id() const override;
  Sequencer& sequencer() const override;
  void run() const override;
  bool shouldMeasureLatencies() const override;

private:
  const std::string id_;
  const SequencerPtr sequencer_;
  const bool measure_latencies_;
  Envoy::TimeSource& time_source_;
  absl::optional<Envoy::MonotonicTime> start_time_;
};

} // namespace Nighthawk