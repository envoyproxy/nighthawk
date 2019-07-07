#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "absl/strings/string_view.h"

namespace Nighthawk {

class MilestoneTracker {
public:
  virtual ~MilestoneTracker() = default;

  /**
   * Reset for re-use.
   */
  virtual void reset() PURE;

  /**
   * Registers a milestone, and returns the index to be used as an argument in further calls
   * related to this milestone.
   */
  virtual uint32_t registerMilestone(absl::string_view name) PURE;

  /**
   * Call when a milestone is reached. Annotatess the milestone with a stimestamp.
   */
  virtual void markMilestone(const uint32_t milestone) PURE;

  /**
   * @return Envoy::MonotonicTime for when the milestone was reached.
   */
  virtual const Envoy::MonotonicTime getMilestone(const uint32_t milestone) const PURE;

  /**
   * @return std::chrono::duration<double> elapsed duration between marking the
   * from and to milestones.
   */
  virtual std::chrono::nanoseconds elapsedBetween(const uint32_t from,
                                                  const uint32_t to) const PURE;
};

using MilestoneTrackerPtr = std::unique_ptr<MilestoneTracker>;

} // namespace Nighthawk
