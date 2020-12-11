#include <chrono>
#include <memory>

#include "nighthawk/common/exception.h"
#include "nighthawk/common/platform_util.h"

#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/test/mocks/event/mocks.h"
#include "external/envoy/test/test_common/simulated_time_system.h"

#include "common/rate_limiter_impl.h"
#include "common/sequencer_impl.h"
#include "common/statistic_impl.h"

#include "test/mocks/common/mock_platform_util.h"
#include "test/mocks/common/mock_rate_limiter.h"
#include "test/mocks/common/mock_termination_predicate.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace nighthawk::client;
using namespace testing;

namespace Nighthawk {

class FakeSequencerTarget {
public:
  virtual ~FakeSequencerTarget() = default;
  // A fake method that matches the sequencer target signature.
  virtual bool callback(OperationCallback) PURE;
};

class MockSequencerTarget : public FakeSequencerTarget {
public:
  MOCK_METHOD1(callback, bool(OperationCallback));
};

class SequencerTestBase : public testing::Test {
public:
  SequencerTestBase()
      : dispatcher_(std::make_unique<Envoy::Event::MockDispatcher>()), frequency_(10_Hz),
        interval_(std::chrono::duration_cast<std::chrono::milliseconds>(frequency_.interval())),
        sequencer_target_(
            std::bind(&SequencerTestBase::callback_test, this, std::placeholders::_1)) {}

  bool callback_test(const OperationCallback& f) {
    callback_test_count_++;
    f(true, true);
    return true;
  }

  MockPlatformUtil platform_util_;
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Event::SimulatedTimeSystem time_system_;
  std::unique_ptr<Envoy::Event::MockDispatcher> dispatcher_;
  int callback_test_count_{0};
  const Frequency frequency_;
  const std::chrono::milliseconds interval_;
  const uint64_t test_number_of_intervals_{5};
  SequencerTarget sequencer_target_;
};

class SequencerTest : public SequencerTestBase {
public:
  SequencerTest()
      : rate_limiter_(std::make_unique<MockRateLimiter>()),
        rate_limiter_unsafe_ref_(*rate_limiter_) {}

  std::unique_ptr<MockRateLimiter> rate_limiter_;
  // The sequencers that the tests construct will take ownership of rate_limiter_, we keep a
  // reference, which will become invalid once the sequencer has been destructed.
  MockRateLimiter& rate_limiter_unsafe_ref_;
};

class SequencerTestWithTimerEmulation : public SequencerTest {
public:
  SequencerTestWithTimerEmulation() { setupDispatcherTimerEmulation(); }

  // the Sequencer implementation is effectively driven by two timers. We set us up for emulating
  // those timers firing and moving simulated time forward in simulateTimerloop() below.
  void setupDispatcherTimerEmulation() {
    timer1_ = new NiceMock<Envoy::Event::MockTimer>();
    timer2_ = new NiceMock<Envoy::Event::MockTimer>();
    EXPECT_CALL(*dispatcher_, createTimer_(_))
        .WillOnce(Invoke([&](Envoy::Event::TimerCb cb) {
          timer_cb_1_ = std::move(cb);
          return timer1_;
        }))
        .WillOnce(Invoke([&](Envoy::Event::TimerCb cb) {
          timer_cb_2_ = std::move(cb);
          return timer2_;
        }));
    EXPECT_CALL(*timer1_, disableTimer()).WillOnce(Invoke([&]() { timer1_set_ = false; }));
    EXPECT_CALL(*timer2_, disableTimer()).WillOnce(Invoke([&]() { timer2_set_ = false; }));
    EXPECT_CALL(*timer1_, enableHRTimer(_, _))
        .WillRepeatedly(Invoke([&](const std::chrono::microseconds,
                                   const Envoy::ScopeTrackedObject*) { timer1_set_ = true; }));
    EXPECT_CALL(*timer2_, enableHRTimer(_, _))
        .WillRepeatedly(Invoke([&](const std::chrono::microseconds,
                                   const Envoy::ScopeTrackedObject*) { timer2_set_ = true; }));
    EXPECT_CALL(*dispatcher_, exit()).WillOnce(Invoke([&]() { stopped_ = true; }));
    EXPECT_CALL(*dispatcher_, updateApproximateMonotonicTime()).Times(AtLeast(1));
    simulation_start_ = time_system_.monotonicTime();
    auto* unsafe_mock_termination_predicate = new MockTerminationPredicate();
    termination_predicate_ =
        std::unique_ptr<MockTerminationPredicate>(unsafe_mock_termination_predicate);
    EXPECT_CALL(*unsafe_mock_termination_predicate, evaluateChain())
        .WillRepeatedly(Invoke([this]() {
          return (time_system_.monotonicTime() - simulation_start_) <=
                         (test_number_of_intervals_ * interval_)
                     ? TerminationPredicate::Status::PROCEED
                     : TerminationPredicate::Status::TERMINATE;
        }));
  }

  void expectDispatcherRun() {
    EXPECT_CALL(*dispatcher_, run(_))
        .WillOnce(Invoke([&](Envoy::Event::DispatcherImpl::RunType type) {
          ASSERT_EQ(Envoy::Event::DispatcherImpl::RunType::RunUntilExit, type);
          simulateTimerLoop();
        }));
  }

  // Moves time forward 1ms, and runs the ballbacks of set timers.
  void simulateTimerLoop() {
    while (!stopped_) {
      time_system_.setMonotonicTime(time_system_.monotonicTime() + NighthawkTimerResolution);

      // TODO(oschaaf): This can be implemented more accurately, by keeping track of timer
      // enablement preserving ordering of which timer should fire first. For now this seems to
      // suffice for the tests that we have in here.
      if (timer1_set_) {
        timer1_set_ = false;
        timer_cb_1_();
      }

      if (timer2_set_) {
        timer2_set_ = false;
        timer_cb_2_();
      }
    }
  }

  MockSequencerTarget* target() { return &target_; }
  TerminationPredicatePtr termination_predicate_;

protected:
  Envoy::MonotonicTime simulation_start_;

private:
  NiceMock<Envoy::Event::MockTimer>* timer1_; // not owned
  NiceMock<Envoy::Event::MockTimer>* timer2_; // not owned
  Envoy::Event::TimerCb timer_cb_1_;
  Envoy::Event::TimerCb timer_cb_2_;
  MockSequencerTarget target_;
  bool timer1_set_{};
  bool timer2_set_{};
  bool stopped_{};
};

// Basic rate limiter interaction test.
TEST_F(SequencerTestWithTimerEmulation, RateLimiterInteraction) {
  SequencerTarget callback =
      std::bind(&MockSequencerTarget::callback, target(), std::placeholders::_1);
  SequencerImpl sequencer(platform_util_, *dispatcher_, time_system_, std::move(rate_limiter_),
                          callback, std::make_unique<StreamingStatistic>(),
                          std::make_unique<StreamingStatistic>(), SequencerIdleStrategy::SLEEP,
                          std::move(termination_predicate_), store_);
  // Have the mock rate limiter gate two calls, and block everything else.
  EXPECT_CALL(rate_limiter_unsafe_ref_, tryAcquireOne())
      .Times(AtLeast(3))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(rate_limiter_unsafe_ref_, elapsed()).Times(2);
  EXPECT_CALL(*target(), callback(_)).Times(2).WillOnce(Return(true)).WillOnce(Return(true));
  expectDispatcherRun();
  EXPECT_CALL(platform_util_, sleep(_)).Times(AtLeast(1));
  sequencer.start();
  sequencer.waitForCompletion();
}

// Saturated rate limiter interaction test.
TEST_F(SequencerTestWithTimerEmulation, RateLimiterSaturatedTargetInteraction) {
  SequencerTarget callback =
      std::bind(&MockSequencerTarget::callback, target(), std::placeholders::_1);
  SequencerImpl sequencer(platform_util_, *dispatcher_, time_system_, std::move(rate_limiter_),
                          callback, std::make_unique<StreamingStatistic>(),
                          std::make_unique<StreamingStatistic>(), SequencerIdleStrategy::SLEEP,
                          std::move(termination_predicate_), store_);

  EXPECT_CALL(rate_limiter_unsafe_ref_, tryAcquireOne())
      .Times(AtLeast(3))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(rate_limiter_unsafe_ref_, elapsed()).Times(2);

  EXPECT_CALL(*target(), callback(_)).Times(2).WillOnce(Return(true)).WillOnce(Return(false));

  // The sequencer should call RateLimiter::releaseOne() when the target returns false.
  EXPECT_CALL(rate_limiter_unsafe_ref_, releaseOne());
  expectDispatcherRun();

  EXPECT_CALL(platform_util_, sleep(_)).Times(AtLeast(1));
  sequencer.start();
  sequencer.waitForCompletion();
}

// The integration tests use a LinearRateLimiter.
class SequencerIntegrationTest : public SequencerTestWithTimerEmulation {
public:
  SequencerIntegrationTest() {
    Envoy::Event::SimulatedTimeSystem time_system;
    rate_limiter_ = std::make_unique<LinearRateLimiter>(time_system_, frequency_);
    expectDispatcherRun();
  }

  bool timeout_test(const std::function<void(bool, bool)>& /* f */) {
    callback_test_count_++;
    // We don't call f(); which will cause the sequencer to think there is in-flight work.
    return true;
  }
  bool saturated_test(const std::function<void(bool, bool)>& /* f */) { return false; }

  std::unique_ptr<LinearRateLimiter> rate_limiter_;

  void testRegularFlow(SequencerIdleStrategy::SequencerIdleStrategyOptions idle_strategy) {
    SequencerImpl sequencer(platform_util_, *dispatcher_, time_system_, std::move(rate_limiter_),
                            sequencer_target_, std::make_unique<StreamingStatistic>(),
                            std::make_unique<StreamingStatistic>(), idle_strategy,
                            std::move(termination_predicate_), store_);
    EXPECT_EQ(0, callback_test_count_);
    EXPECT_EQ(0, sequencer.latencyStatistic().count());
    sequencer.start();
    sequencer.waitForCompletion();
    EXPECT_EQ(test_number_of_intervals_, callback_test_count_);
    EXPECT_EQ(test_number_of_intervals_, sequencer.latencyStatistic().count());
    EXPECT_EQ(0, sequencer.blockedStatistic().count());
    EXPECT_EQ(2, sequencer.statistics().size());
    const auto execution_duration = time_system_.monotonicTime() - simulation_start_;
    EXPECT_EQ(sequencer.executionDuration(), execution_duration);
  }
};

TEST_F(SequencerIntegrationTest, IdleStrategySpin) {
  EXPECT_CALL(platform_util_, yieldCurrentThread()).Times(AtLeast(1));
  EXPECT_CALL(platform_util_, sleep(_)).Times(0);
  testRegularFlow(SequencerIdleStrategy::SPIN);
}

TEST_F(SequencerIntegrationTest, IdleStrategyPoll) {
  EXPECT_CALL(platform_util_, yieldCurrentThread()).Times(0);
  EXPECT_CALL(platform_util_, sleep(_)).Times(0);
  testRegularFlow(SequencerIdleStrategy::POLL);
}

TEST_F(SequencerIntegrationTest, IdleStrategySleep) {
  EXPECT_CALL(platform_util_, yieldCurrentThread()).Times(0);
  EXPECT_CALL(platform_util_, sleep(_)).Times(AtLeast(1));
  testRegularFlow(SequencerIdleStrategy::SLEEP);
}

// Test an always saturated sequencer target. A concrete example would be a http benchmark client
// not being able to start any requests, for example due to misconfiguration or system conditions.
TEST_F(SequencerIntegrationTest, AlwaysSaturatedTargetTest) {
  SequencerTarget callback =
      std::bind(&SequencerIntegrationTest::saturated_test, this, std::placeholders::_1);
  SequencerImpl sequencer(platform_util_, *dispatcher_, time_system_, std::move(rate_limiter_),
                          callback, std::make_unique<StreamingStatistic>(),
                          std::make_unique<StreamingStatistic>(), SequencerIdleStrategy::SLEEP,
                          std::move(termination_predicate_), store_);
  EXPECT_CALL(platform_util_, sleep(_)).Times(AtLeast(1));
  sequencer.start();
  sequencer.waitForCompletion();

  EXPECT_EQ(0, sequencer.latencyStatistic().count());
  EXPECT_EQ(1, sequencer.blockedStatistic().count());
}

// (SequencerIntegrationTest::timeout_test()) will never call back, effectively simulated a
// stalled benchmark client. Implicitly we test that we get past sequencer.waitForCompletion()
// timely, and don't hang.
TEST_F(SequencerIntegrationTest, CallbacksDoNotInfluenceTestDuration) {
  SequencerTarget callback =
      std::bind(&SequencerIntegrationTest::timeout_test, this, std::placeholders::_1);
  SequencerImpl sequencer(platform_util_, *dispatcher_, time_system_, std::move(rate_limiter_),
                          callback, std::make_unique<StreamingStatistic>(),
                          std::make_unique<StreamingStatistic>(), SequencerIdleStrategy::SLEEP,
                          std::move(termination_predicate_), store_);
  EXPECT_CALL(platform_util_, sleep(_)).Times(AtLeast(1));
  auto pre_timeout = time_system_.monotonicTime();
  sequencer.start();
  sequencer.waitForCompletion();

  auto diff = time_system_.monotonicTime() - pre_timeout;

  auto expected_duration = (test_number_of_intervals_ * interval_) + NighthawkTimerResolution;
  EXPECT_EQ(expected_duration, diff);

  // the test itself should have seen all callbacks...
  EXPECT_EQ(5, callback_test_count_);
  // ... but they ought to have not arrived at the Sequencer.
  EXPECT_EQ(0, sequencer.latencyStatistic().count());
  EXPECT_EQ(0, sequencer.blockedStatistic().count());
}

} // namespace Nighthawk
