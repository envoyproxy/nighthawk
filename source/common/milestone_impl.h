
#pragma once

#include "envoy/common/time.h"

#include "nighthawk/common/milestone.h"

namespace Nighthawk {

class MilestoneImpl : public Milestone {
public:
  MilestoneImpl(const Envoy::MonotonicTime& time, const char* name) : time_(time), name_(name) {}
  const Envoy::MonotonicTime& time() const override;
  const char* name() const override;

private:
  Envoy::MonotonicTime time_;
  const char* name_;
};

class MilestoneTrackerImpl : public MilestoneTracker {
public:
  MilestoneTrackerImpl(const MilestoneCallback& callback, Envoy::TimeSource& time_source)
      : callback_(callback), time_source_(time_source) {}
  ~MilestoneTrackerImpl() override;
  void addMilestone(const char* name) override;

private:
  MilestoneCallback callback_;
  Envoy::TimeSource& time_source_;
  MilestoneCollection milestones_;
};

} // namespace Nighthawk