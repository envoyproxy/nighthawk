#include <memory>

#include "nighthawk/common/exception.h"

#include "common/milestone_tracker_impl.h"
#include "common/pool_impl.h"
#include "common/poolable_impl.h"

#include "test/test_common/simulated_time_system.h"

#include "gtest/gtest.h"

using namespace testing;
using namespace std::chrono_literals;

namespace Nighthawk {

class MilestoneTrackerTest : public testing::Test {
public:
  void basicRun() {
    tracker_->markMilestone(MilestoneTracker::Milestone::Start);
    time_system_.sleep(1s);
    tracker_->markMilestone(MilestoneTracker::Milestone::Complete);
    time_system_.sleep(1s);

    EXPECT_EQ(tracker_->elapsedBetween(MilestoneTracker::Milestone::Start,
                                       MilestoneTracker::Milestone::Complete),
              1s);
  }

  Envoy::Event::SimulatedTimeSystem time_system_;
  MilestoneTrackerPtr tracker_ = std::make_unique<MilestoneTrackerImpl>(time_system_);
};

TEST_F(MilestoneTrackerTest, Tracking) { basicRun(); }

TEST_F(MilestoneTrackerTest, Reset) {
  basicRun();
  tracker_->reset();
  basicRun();
}

TEST_F(MilestoneTrackerTest, SameMilestoneTwiceThrows) {
  tracker_->markMilestone(MilestoneTracker::Milestone::Start);
  EXPECT_THROW(tracker_->markMilestone(MilestoneTracker::Milestone::Start), NighthawkException);
  tracker_->markMilestone(MilestoneTracker::Milestone::BlockingStart);
  EXPECT_THROW(tracker_->markMilestone(MilestoneTracker::Milestone::Start), NighthawkException);
  EXPECT_THROW(tracker_->markMilestone(MilestoneTracker::Milestone::BlockingStart),
               NighthawkException);
}

TEST_F(MilestoneTrackerTest, OutOfOrderMilsetoneSetThrows) {
  tracker_->markMilestone(MilestoneTracker::Milestone::Start);
  tracker_->markMilestone(MilestoneTracker::Milestone::BlockingStart);
  EXPECT_EQ(tracker_->elapsedBetween(MilestoneTracker::Milestone::Start,
                                     MilestoneTracker::Milestone::BlockingStart),
            0s);
  EXPECT_THROW(tracker_->elapsedBetween(MilestoneTracker::Milestone::BlockingStart,
                                        MilestoneTracker::Milestone::Start),
               NighthawkException);
}

} // namespace Nighthawk
