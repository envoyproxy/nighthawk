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
  enum Milestone { Start = 0, Middle, End };

  void SetUp() override {
    // Corresponds with the enum above.
    EXPECT_EQ(0, tracker_->registerMilestone("start"));
    EXPECT_EQ(1, tracker_->registerMilestone("middle"));
    EXPECT_EQ(2, tracker_->registerMilestone("end"));
  }

  void basicRun() {
    tracker_->markMilestone(Milestone::Start);
    time_system_.sleep(1s);
    tracker_->markMilestone(Milestone::Middle);
    time_system_.sleep(1s);
    tracker_->markMilestone(Milestone::End);
    EXPECT_EQ(tracker_->elapsedBetween(Milestone::Start, Milestone::Middle), 1s);
    EXPECT_EQ(tracker_->elapsedBetween(Milestone::Middle, Milestone::End), 1s);
    EXPECT_EQ(tracker_->elapsedBetween(Milestone::Start, Milestone::End), 2s);
  }

  Envoy::Event::SimulatedTimeSystem time_system_;
  MilestoneTrackerPtr tracker_ = std::make_unique<MilestoneTrackerImpl>(time_system_);
};

TEST_F(MilestoneTrackerTest, Tracking) { basicRun(); }

TEST_F(MilestoneTrackerTest, CanReuseAfterReset) {
  basicRun();
  tracker_->reset();
  basicRun();
}

TEST_F(MilestoneTrackerTest, SameMilestoneTwiceThrows) {
  tracker_->markMilestone(Milestone::Start);
  EXPECT_THROW(tracker_->markMilestone(Milestone::Start), NighthawkException);
  tracker_->markMilestone(Milestone::Middle);
  EXPECT_THROW(tracker_->markMilestone(Milestone::Start), NighthawkException);
  EXPECT_THROW(tracker_->markMilestone(Milestone::Middle), NighthawkException);
}

TEST_F(MilestoneTrackerTest, OutOfOrderMilsetoneQueryThrows) {
  tracker_->markMilestone(Milestone::Start);
  tracker_->markMilestone(Milestone::Middle);
  EXPECT_EQ(tracker_->elapsedBetween(Milestone::Start, Milestone::Middle), 0s);
  EXPECT_THROW(tracker_->elapsedBetween(Milestone::End, Milestone::Start), NighthawkException);
}

} // namespace Nighthawk
