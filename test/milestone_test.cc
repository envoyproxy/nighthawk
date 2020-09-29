#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/test/test_common/simulated_time_system.h"

#include "common/milestone_impl.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using namespace std::chrono_literals;

class MilestoneTestWithSimtime : public testing::Test, public Envoy::Event::TestUsingSimulatedTime {
public:
  Envoy::Event::SimulatedTimeSystem& time_system_ = simTime();
};

TEST_F(MilestoneTestWithSimtime, BasicTest) {
  MilestoneTrackerImpl m(
      [](const MilestoneCollection& milestones) {
        ASSERT_EQ(milestones.size(), 4);
        EXPECT_EQ(milestones[0]->time().time_since_epoch(), 0s);
        EXPECT_EQ(milestones[1]->time().time_since_epoch(), 1s);
        EXPECT_EQ(milestones[2]->time().time_since_epoch(), 1s);
        EXPECT_EQ(milestones[3]->time().time_since_epoch(), 2s);
      },
      time_system_);

  m.addMilestone("no time elapsed");
  time_system_.setMonotonicTime(1s);
  m.addMilestone("1 second elapsed");
  m.addMilestone("0 seconds elapsed before callback");
  time_system_.setMonotonicTime(2s);
  m.addMilestone("1 seconds elapsed");
}

TEST(Benchmark, DISABLED_VerySimpleSpeedTest) {
  Envoy::Event::RealTimeSystem time_system;
  const uint64_t kIterations = 1000000;

  MilestoneTrackerImpl tracker(
      [](const MilestoneCollection& milestones) {
        std::cerr << "done"
                  << ((milestones[1]->time() - milestones[0]->time()).count() / kIterations)
                  << "ns/iteration." << std::endl;
      },
      time_system);

  tracker.addMilestone("start");
  for (uint64_t i = 0; i < kIterations; i++) {
    MilestoneTrackerImpl m([](const MilestoneCollection&) {}, time_system);
    m.addMilestone("no time elapsed");
    m.addMilestone("1 second elapsed");
    m.addMilestone("0 seconds elapsed before callback");
    m.addMilestone("1 seconds elapsed");
  }
  tracker.addMilestone("done");
}

} // namespace
} // namespace Nighthawk
