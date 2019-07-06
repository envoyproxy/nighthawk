#include <chrono>

#include "common/milestone_tracker_impl.h"
#include "common/pool_impl.h"
#include "common/poolable_impl.h"

#include "test/mocks.h"
#include "test/test_common/simulated_time_system.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {

class MockPoolablePoolImpl : public PoolImpl<MockPoolable> {
public:
  MockPoolablePoolImpl(
      MockPoolablePoolImpl::PoolInstanceConstructionDelegate&& construction_delegate,
      MockPoolablePoolImpl::PoolInstanceResetDelegate&& reset_delegate)
      : PoolImpl<MockPoolable>(std::move(construction_delegate), std::move(reset_delegate)) {}

  MockPoolablePoolImpl() = default;
};
class PoolTest : public testing::Test {};

TEST_F(PoolTest, DestructPoolWithoutInFlightPoolables) {
  auto pool = std::make_unique<MockPoolablePoolImpl>();
  EXPECT_EQ(0, pool->allocated());
  pool->addPoolable(std::make_unique<MockPoolable>());
  EXPECT_EQ(1, pool->allocated());
  EXPECT_EQ(1, pool->available());
  auto poolable = pool->get();
  EXPECT_CALL(*poolable, is_orphaned()).WillOnce(Return(false));
  EXPECT_EQ(1, pool->allocated());
  EXPECT_EQ(0, pool->available());
  poolable = nullptr;
  EXPECT_EQ(1, pool->allocated());
  EXPECT_EQ(1, pool->available());
}

TEST_F(PoolTest, DestructPoolWithInFlightPoolables) {
  auto pool = std::make_unique<MockPoolablePoolImpl>();
  EXPECT_EQ(0, pool->allocated());
  EXPECT_EQ(0, pool->available());

  pool->addPoolable(std::make_unique<MockPoolable>());
  EXPECT_EQ(1, pool->allocated());
  EXPECT_EQ(1, pool->available());

  MockPoolablePoolImpl::PoolablePtr poolable = pool->get();
  EXPECT_EQ(1, pool->allocated());
  EXPECT_EQ(0, pool->available());

  // We will reset the pool, which should cause it to call mark_orphaned() on the in-flight
  // poolable object.
  EXPECT_CALL(*poolable, mark_orphaned());

  // As is_orphaned is set have it return true so it will self destruct on test exit.
  pool.reset();
  EXPECT_CALL(*poolable, is_orphaned()).WillOnce(Return(true));
}

TEST_F(PoolTest, AllocationDelegate) {
  auto pool = std::make_unique<MockPoolablePoolImpl>([]() { return new MockPoolable(); }, nullptr);
  EXPECT_EQ(0, pool->allocated());
  EXPECT_EQ(0, pool->available());

  MockPoolablePoolImpl::PoolablePtr poolable = pool->get();
  EXPECT_CALL(*poolable, is_orphaned()).WillOnce(Return(false));

  EXPECT_EQ(1, pool->allocated());
  EXPECT_EQ(0, pool->available());
}

// Compose a poolable milestone for testing a concrete mix-in case.
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

class MilestoneTrackerPoolTest : public testing::Test {
public:
  Envoy::Event::SimulatedTimeSystem time_system_;
};

// Hello world test with a concrete poolable implementing object.
TEST_F(MilestoneTrackerPoolTest, HappyPoolImpl) {
  MilestoneTrackerPoolImpl pool;
  pool.addPoolable(std::make_unique<PoolableMilestoneTrackerImpl>(time_system_));
  auto milestone = pool.get();
  milestone->reset();
}

TEST_F(MilestoneTrackerPoolTest, ResetDelegate) {
  int reset_count = 0;
  auto pool = std::make_unique<MilestoneTrackerPoolImpl>(
      [this]() { return new PoolableMilestoneTrackerImpl(time_system_); },
      [&reset_count](PoolableMilestoneTrackerImpl&) { reset_count++; });
  MilestoneTrackerPoolImpl::PoolablePtr poolable = pool->get();
  poolable = nullptr;
  EXPECT_EQ(1, reset_count);
}

} // namespace Nighthawk
