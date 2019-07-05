#pragma once

#include <string>
#include <vector>

#include "envoy/common/time.h"

#include "nighthawk/common/milestone_tracker.h"

namespace Nighthawk {

class MilestoneTrackerImpl : public MilestoneTracker {
public:
  MilestoneTrackerImpl(Envoy::TimeSource& time_source);
  void reset() override;
  uint32_t registerMilestone(absl::string_view name) override;
  void markMilestone(const uint32_t milestone) override;
  const Envoy::MonotonicTime getMilestone(const uint32_t milestone) const override;
  std::chrono::duration<double> elapsedBetween(const uint32_t from,
                                               const uint32_t to) const override;

private:
  Envoy::TimeSource& time_source_;
  std::vector<std::tuple<Envoy::MonotonicTime, std::string>> timestamps_;
};

} // namespace Nighthawk
