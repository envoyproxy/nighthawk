#include "common/milestone_tracker_impl.h"

#include "nighthawk/common/exception.h"

#include "common/common/assert.h"

namespace Nighthawk {

MilestoneTrackerImpl::MilestoneTrackerImpl(Envoy::TimeSource& time_source)
    : time_source_(time_source) {}

void MilestoneTrackerImpl::reset() {
  for (auto& timestamp : timestamps_) {
    std::get<0>(timestamp) = Envoy::MonotonicTime::min();
  }
}

uint32_t MilestoneTrackerImpl::registerMilestone(absl::string_view name) {
  timestamps_.emplace_back(std::tuple<Envoy::MonotonicTime, std::string>{
      Envoy::MonotonicTime::min(), std::string(name)});
  return timestamps_.size() - 1;
}

void MilestoneTrackerImpl::markMilestone(const uint32_t milestone) {
  ASSERT(milestone < timestamps_.size());
  auto& timestamp = std::get<0>(timestamps_[milestone]);
  if (timestamp != Envoy::MonotonicTime::min()) {
    throw NighthawkException("Milestone already set");
  }
  timestamp = time_source_.monotonicTime();
}

const Envoy::MonotonicTime MilestoneTrackerImpl::getMilestone(const uint32_t milestone) const {
  return std::get<0>(timestamps_[milestone]);
}

std::chrono::duration<double> MilestoneTrackerImpl::elapsedBetween(const uint32_t from,
                                                                   const uint32_t to) const {
  if (from >= to) {
    throw NighthawkException("The 'to' milestone must lie ahead of 'to'.");
  }
  return getMilestone(to) - getMilestone(from);
}

} // namespace Nighthawk
