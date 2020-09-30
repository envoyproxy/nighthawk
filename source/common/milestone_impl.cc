#include "common/milestone_impl.h"

#include <iostream>

#include "external/envoy/source/common/common/assert.h"

namespace Nighthawk {

MilestoneTrackerImpl::~MilestoneTrackerImpl() { callback_(milestones_); }

void MilestoneTrackerImpl::addMilestone(const char* name) {
  dispatcher_.updateApproximateMonotonicTime();
  milestones_.emplace_back(std::make_unique<MilestoneImpl>(time_source_.monotonicTime(), name));
}

const Envoy::MonotonicTime& MilestoneImpl::time() const { return time_; }
const char* MilestoneImpl::name() const { return name_; }

} // namespace Nighthawk