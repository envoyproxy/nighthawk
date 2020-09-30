
#pragma once

#include "envoy/common/time.h"
#include "envoy/event/dispatcher.h"

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
  MilestoneTrackerImpl(const MilestoneCallback& callback, Envoy::TimeSource& time_source,
                       Envoy::Event::Dispatcher& dispatcher)
      : callback_(callback), time_source_(time_source), dispatcher_(dispatcher) {}
  ~MilestoneTrackerImpl() override;
  void addMilestone(const char* name) override;

private:
  MilestoneCallback callback_;
  Envoy::TimeSource& time_source_;
  MilestoneCollection milestones_;
  Envoy::Event::Dispatcher& dispatcher_;
};

class NullMilestoneTrackerImpl : public MilestoneTracker {
public:
  void addMilestone(const char*) override{/*TODO(oschaaf): gcovr not implemented here*/};
};

} // namespace Nighthawk