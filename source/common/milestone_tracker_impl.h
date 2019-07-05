#pragma once

#include <vector>

#include "envoy/common/time.h"

#include "nighthawk/common/milestone_tracker.h"

#include "common/common/assert.h"

namespace Nighthawk {

class MilestoneTrackerImpl : public MilestoneTracker {
public:
  MilestoneTrackerImpl(Envoy::TimeSource& time_source);
  void reset() override;
  void markMilestone(const Milestone milestone) override;
  const Envoy::MonotonicTime getMilestone(const Milestone milestone) const override;
  std::chrono::duration<double> elapsedBetween(const Milestone from,
                                               const Milestone to) const override;

private:
  Envoy::TimeSource& time_source_;
  std::vector<std::tuple<Milestone, Envoy::MonotonicTime>> timestamps_;
};

} // namespace Nighthawk
