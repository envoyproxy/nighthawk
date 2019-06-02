#include <chrono>
#include <memory>

#include "nighthawk/common/exception.h"
#include "nighthawk/common/platform_util.h"

#include "common/api/api_impl.h"
#include "common/common/thread_impl.h"
#include "common/event/dispatcher_impl.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/rate_limiter_impl.h"
#include "common/sequencer_impl.h"
#include "common/statistic_impl.h"
#include "common/stats/isolated_store_impl.h"

#include "test/mocks.h"
#include "test/mocks/event/mocks.h"
#include "test/test_common/simulated_time_system.h"
#include "test/test_common/thread_factory_for_test.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {

class SequencerTestBase : public Test {
public:
  SequencerTestBase()
      : api_(Envoy::Thread::threadFactoryForTest(), store_, time_system_, file_system_),
        dispatcher_(std::make_unique<Envoy::Event::MockDispatcher>()), frequency_(10_Hz),
        interval_(std::chrono::duration_cast<std::chrono::milliseconds>(frequency_.interval())),
        sequencer_target_(
            std::bind(&SequencerTestBase::callback_test, this, std::placeholders::_1)) {}

  bool callback_test(const std::function<void()>& f) {
    callback_test_count_++;
    f();
    return true;
  }

  Envoy::Filesystem::InstanceImplPosix file_system_;
  MockPlatformUtil platform_util_;
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Event::SimulatedTimeSystem time_system_;
  Envoy::Api::Impl api_;
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
    EXPECT_CALL(*timer1_, enableTimer(_)).WillRepeatedly(Invoke([&](std::chrono::milliseconds) {
      timer1_set_ = true;
    }));
    EXPECT_CALL(*timer2_, enableTimer(_)).WillRepeatedly(Invoke([&](std::chrono::milliseconds) {
      timer2_set_ = true;
    }));
    EXPECT_CALL(*dispatcher_, exit()).WillOnce(Invoke([&]() { stopped_ = true; }));
  }

  void expectDispatcherRun() {
    EXPECT_CALL(*dispatcher_, run(_))
        .WillOnce(Invoke([&](Envoy::Event::DispatcherImpl::RunType type) {
          ASSERT_EQ(Envoy::Event::DispatcherImpl::RunType::Block, type);
          simulateTimerLoop();
        }));
  }

  // Moves time forward 1ms, and runs the ballbacks of set timers.
  void simulateTimerLoop() {
    while (!stopped_) {
      time_system_.setMonotonicTime(time_system_.monotonicTime() + EnvoyTimerMinResolution);

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
  SequencerImpl sequencer(
      platform_util_, *dispatcher_, time_system_, time_system_.monotonicTime(),
      std::move(rate_limiter_), callback, std::make_unique<StreamingStatistic>(),
      std::make_unique<StreamingStatistic>(),
      test_number_of_intervals_ * interval_ /* Sequencer run time.*/, 1ms /* Sequencer timeout. */);
  // Have the mock rate limiter gate two calls, and block everything else.
  EXPECT_CALL(rate_limiter_unsafe_ref_, tryAcquireOne())
      .Times(AtLeast(3))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*target(), callback(_)).Times(2).WillOnce(Return(true)).WillOnce(Return(true));
  expectDispatcherRun();
  sequencer.start();
  sequencer.waitForCompletion();
}

TEST_F(SequencerTestWithTimerEmulation, StartingLate) {
  SequencerTarget callback =
      std::bind(&MockSequencerTarget::callback, target(), std::placeholders::_1);
  SequencerImpl sequencer(
      platform_util_, *dispatcher_, time_system_, time_system_.monotonicTime(),
      std::move(rate_limiter_), callback, std::make_unique<StreamingStatistic>(),
      std::make_unique<StreamingStatistic>(),
      test_number_of_intervals_ * interval_ /* Sequencer run time.*/, 1ms /* Sequencer timeout. */);

  time_system_.setMonotonicTime(time_system_.monotonicTime() + 100s);
  sequencer.start();
  sequencer.waitForCompletion();
}

// Saturated rate limiter interaction test.
TEST_F(SequencerTestWithTimerEmulation, RateLimiterSaturatedTargetInteraction) {
  SequencerTarget callback =
      std::bind(&MockSequencerTarget::callback, target(), std::placeholders::_1);
  SequencerImpl sequencer(
      platform_util_, *dispatcher_, time_system_, time_system_.monotonicTime(),
      std::move(rate_limiter_), callback, std::make_unique<StreamingStatistic>(),
      std::make_unique<StreamingStatistic>(),
      test_number_of_intervals_ * interval_ /* Sequencer run time.*/, 0ms /* Sequencer timeout. */);

  EXPECT_CALL(rate_limiter_unsafe_ref_, tryAcquireOne())
      .Times(AtLeast(3))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*target(), callback(_)).Times(2).WillOnce(Return(true)).WillOnce(Return(false));

  // The sequencer should call RateLimiter::releaseOne() when the target returns false.
  EXPECT_CALL(rate_limiter_unsafe_ref_, releaseOne()).Times(1);
  expectDispatcherRun();

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

  bool timeout_test(const std::function<void()>& /* f */) {
    callback_test_count_++;
    // We don't call f(); which will cause the sequencer to think there is in-flight work.
    return true;
  }
  bool saturated_test(const std::function<void()>& /* f */) { return false; }

  std::unique_ptr<LinearRateLimiter> rate_limiter_;
};

TEST_F(SequencerIntegrationTest, TheHappyFlow) {
  SequencerImpl sequencer(
      platform_util_, *dispatcher_, time_system_, time_system_.monotonicTime(),
      std::move(rate_limiter_), sequencer_target_, std::make_unique<StreamingStatistic>(),
      std::make_unique<StreamingStatistic>(), test_number_of_intervals_ * interval_, 1s);

  EXPECT_CALL(platform_util_, yieldCurrentThread()).Times(AtLeast(1));

  EXPECT_EQ(0, callback_test_count_);
  EXPECT_EQ(0, sequencer.latencyStatistic().count());

  sequencer.start();
  sequencer.waitForCompletion();

  EXPECT_EQ(test_number_of_intervals_, callback_test_count_);
  EXPECT_EQ(test_number_of_intervals_, sequencer.latencyStatistic().count());
  EXPECT_EQ(0, sequencer.blockedStatistic().count());

  EXPECT_EQ(2, sequencer.statistics().size());
}

// TODO(oschaaf): would be good to have a mid-run cancellation test as well.
TEST_F(SequencerIntegrationTest, CancelEarly) {
  SequencerImpl sequencer(
      platform_util_, *dispatcher_, time_system_, time_system_.monotonicTime(),
      std::move(rate_limiter_), sequencer_target_, std::make_unique<StreamingStatistic>(),
      std::make_unique<StreamingStatistic>(), test_number_of_intervals_ * interval_, 1s);

  EXPECT_EQ(0, callback_test_count_);
  EXPECT_EQ(0, sequencer.latencyStatistic().count());

  sequencer.start();
  sequencer.cancel();
  sequencer.waitForCompletion();

  EXPECT_EQ(0, callback_test_count_);
  EXPECT_EQ(0, sequencer.latencyStatistic().count());
  EXPECT_EQ(0, sequencer.blockedStatistic().count());

  EXPECT_EQ(2, sequencer.statistics().size());
}

// Test an always saturated sequencer target. A concrete example would be a http benchmark client
// not being able to start any requests, for example due to misconfiguration or system conditions.
TEST_F(SequencerIntegrationTest, AlwaysSaturatedTargetTest) {
  SequencerTarget callback =
      std::bind(&SequencerIntegrationTest::saturated_test, this, std::placeholders::_1);
  SequencerImpl sequencer(
      platform_util_, *dispatcher_, time_system_, time_system_.monotonicTime(),
      std::move(rate_limiter_), callback, std::make_unique<StreamingStatistic>(),
      std::make_unique<StreamingStatistic>(),
      test_number_of_intervals_ * interval_ /* Sequencer run time.*/, 1ms /* Sequencer timeout. */);

  sequencer.start();
  sequencer.waitForCompletion();

  EXPECT_EQ(0, sequencer.latencyStatistic().count());
  EXPECT_EQ(1, sequencer.blockedStatistic().count());
}

// Test the (grace-)-timeout feature of the Sequencer. The used sequencer target
// (SequencerIntegrationTest::timeout_test()) will never call back, effectively simulated a
// stalled benchmark client. Implicitly we test  that we get past
// sequencer.waitForCompletion(), which would only hold when sequencer enforces the the timeout.
TEST_F(SequencerIntegrationTest, GraceTimeoutTest) {
  auto grace_timeout = 12345ms;

  SequencerTarget callback =
      std::bind(&SequencerIntegrationTest::timeout_test, this, std::placeholders::_1);
  SequencerImpl sequencer(
      platform_util_, *dispatcher_, time_system_, time_system_.monotonicTime(),
      std::move(rate_limiter_), callback, std::make_unique<StreamingStatistic>(),
      std::make_unique<StreamingStatistic>(),
      test_number_of_intervals_ * interval_ /* Sequencer run time.*/, grace_timeout);

  auto pre_timeout = time_system_.monotonicTime();
  sequencer.start();
  sequencer.waitForCompletion();

  auto diff = time_system_.monotonicTime() - pre_timeout;

  auto expected_duration =
      (test_number_of_intervals_ * interval_) + grace_timeout + EnvoyTimerMinResolution;
  EXPECT_EQ(expected_duration, diff);

  // the test itself should have seen all callbacks...
  EXPECT_EQ(5, callback_test_count_);
  // ... but they ought to have not arrived at the Sequencer.
  EXPECT_EQ(0, sequencer.latencyStatistic().count());
  EXPECT_EQ(0, sequencer.blockedStatistic().count());
}

} // namespace Nighthawk
