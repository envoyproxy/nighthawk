
#pragma once

#include <memory>

#include "envoy/common/pure.h"

#include "nighthawk/common/sequencer.h"

namespace Nighthawk {

/**
 * Phase represents a distinct phase of a benchmmark execution, like warmup and cooldown.
 * A phase is associated to a sequencer, which in turn can be associated to separate termination
 * and failure predicates as well as its own rate limiter policy. The end of a phase also represents
 * a natural boundary for reporting a snapshot of stats and latencies associated to the phase.
 * High level, a worker statically configure a vector of phases, and will transfer the hot
 * connection pool when transitioning between them. At this time, nothing is stopping us from
 * dynamically injecting phases later, be it via grpc calls and/or live CLI input.
 */
class Phase {
public:
  virtual ~Phase() = default;

  /**
   * @return absl::string_view Contains the id of the phase. Should be unique but that is not
   * enforced at this time so take care.
   */
  virtual absl::string_view id() const PURE;

  /**
   * @return Sequencer& Sequencer associated to this phase.
   */
  virtual Sequencer& sequencer() const PURE;

  /**
   * @return bool Indicates if latencies should be tracked for this phase.
   */
  virtual bool shouldMeasureLatencies() const PURE;

  /**
   * Runs the sequencer associated to this phase and blocks until completion, which means this phase
   * has ended as well.
   * Execution failure can be observed via the sequencer.failed_terminations counter.
   */
  virtual void run() const PURE;
};

using PhasePtr = std::unique_ptr<Phase>;

} // namespace Nighthawk