#include "common/milestone_tracker_impl.h"

#include "nighthawk/common/exception.h"

namespace Nighthawk {

MilestoneTrackerImpl::MilestoneTrackerImpl(Envoy::TimeSource& time_source)
    : time_source_(time_source) {
  for (int i = 0; i <= static_cast<int>(Milestone::Complete); i++) {
    timestamps_.emplace_back(
        std::tuple<Milestone, Envoy::MonotonicTime>{Milestone(i), Envoy::MonotonicTime::min()});
  }
}

void MilestoneTrackerImpl::reset() {
  for (int i = 0; i <= static_cast<int>(Milestone::Complete); i++) {
    std::get<1>(timestamps_[i]) = Envoy::MonotonicTime::min();
  }
}

void MilestoneTrackerImpl::markMilestone(const Milestone milestone) {
  auto& timestamp = std::get<1>(timestamps_[static_cast<int>(milestone)]);
  if (timestamp != Envoy::MonotonicTime::min()) {
    throw NighthawkException(fmt::format("Milestone {} already set", static_cast<int>(milestone)));
  }
  timestamp = time_source_.monotonicTime();
}

const Envoy::MonotonicTime MilestoneTrackerImpl::getMilestone(const Milestone milestone) const {
  return std::get<1>(timestamps_[static_cast<int>(milestone)]);
}

std::chrono::duration<double> MilestoneTrackerImpl::elapsedBetween(const Milestone from,
                                                                   const Milestone to) const {
  if (static_cast<int>(from) >= static_cast<int>(to)) {
    throw NighthawkException("The 'to' milestone must lie ahead of 'to'.");
  }
  return getMilestone(to) - getMilestone(from);
}

} // namespace Nighthawk
