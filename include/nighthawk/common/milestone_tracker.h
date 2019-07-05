#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "nighthawk/common/poolable.h"

namespace Nighthawk {

class MilestoneTracker {
public:
  enum class Milestone {
    Start = 0,
    SequencerStart,
    SequencerInit,
    BlockingStart,
    BlockingEnd,
    TransactionStart,
    TransactionEnd,
    Complete
  };

  virtual ~MilestoneTracker() = default;

  /**
   * Reset for re-use.
   */
  virtual void reset() PURE;

  /**
   * Call when a milestone is reached. Annotatess the milestone with a stimestamp.
   */
  virtual void markMilestone(const Milestone milestone) PURE;

  /**
   * @return Envoy::MonotonicTime for when the milestone was reached.
   */
  virtual const Envoy::MonotonicTime getMilestone(const Milestone milestone) const PURE;

  /**
   * @return std::chrono::duration<double> elapsed duration between marking the
   * from and to milestones.
   */
  virtual std::chrono::duration<double> elapsedBetween(const Milestone from,
                                                       const Milestone to) const PURE;
};

using MilestoneTrackerPtr = std::unique_ptr<MilestoneTracker>;

} // namespace Nighthawk
