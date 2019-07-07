#pragma once

#include <string>
#include <vector>

#include "envoy/common/time.h"

#include "nighthawk/common/milestone_tracker.h"

#include "common/pool_impl.h"
#include "common/poolable_impl.h"

namespace Nighthawk {

class MilestoneTrackerImpl : public MilestoneTracker {
public:
  MilestoneTrackerImpl(Envoy::TimeSource& time_source);
  void reset() override;
  uint32_t registerMilestone(absl::string_view name) override;
  void markMilestone(const uint32_t milestone) override;
  const Envoy::MonotonicTime getMilestone(const uint32_t milestone) const override;
  std::chrono::nanoseconds elapsedBetween(const uint32_t from, const uint32_t to) const override;

private:
  Envoy::TimeSource& time_source_;
  std::vector<std::tuple<Envoy::MonotonicTime, std::string>> timestamps_;
  int last_milestone_{-1}; // for verification only.
};

// Compose a poolable MilestoneTrackerImpl.
class PoolableMilestoneTrackerImpl : public MilestoneTrackerImpl, public PoolableImpl {
public:
  PoolableMilestoneTrackerImpl(Envoy::TimeSource& time_source)
      : MilestoneTrackerImpl(time_source) {}
};

// Declare a pool for the poolable milestone
class MilestoneTrackerPoolImpl : public PoolImpl<PoolableMilestoneTrackerImpl> {
public:
  MilestoneTrackerPoolImpl(
      MilestoneTrackerPoolImpl::PoolInstanceConstructionDelegate&& construction_delegate,
      MilestoneTrackerPoolImpl::PoolInstanceResetDelegate&& reset_delegate)
      : PoolImpl<PoolableMilestoneTrackerImpl>(std::move(construction_delegate),
                                               std::move(reset_delegate)) {}
  MilestoneTrackerPoolImpl() = default;
};

} // namespace Nighthawk
