#include <chrono>
#include <vector>

#include "nighthawk/common/exception.h"

#include "external/envoy/test/test_common/simulated_time_system.h"

#include "common/frequency.h"
#include "common/rate_limiter_impl.h"

#include "test/mocks/common/mock_rate_limiter.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {

class RateLimiterTest : public Test {};

TEST_F(RateLimiterTest, LinearRateLimiterTest) {
  Envoy::Event::SimulatedTimeSystem time_system;
  // Construct a 10/second paced rate limiter.
  LinearRateLimiter rate_limiter(time_system, 10_Hz);

  EXPECT_FALSE(rate_limiter.tryAcquireOne());

  time_system.advanceTimeWait(100ms);
  EXPECT_TRUE(rate_limiter.tryAcquireOne());
  EXPECT_FALSE(rate_limiter.tryAcquireOne());

  time_system.advanceTimeWait(1s);
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(rate_limiter.tryAcquireOne());
  }
  EXPECT_FALSE(rate_limiter.tryAcquireOne());
}

TEST_F(RateLimiterTest, LinearRateLimiterInvalidArgumentTest) {
  Envoy::Event::SimulatedTimeSystem time_system;
  EXPECT_THROW(LinearRateLimiter rate_limiter(time_system, 0_Hz), NighthawkException);
}

TEST_F(RateLimiterTest, BurstingRateLimiterTest) {
  const uint64_t burst_size = 3;
  std::unique_ptr<MockRateLimiter> mock_rate_limiter = std::make_unique<MockRateLimiter>();
  MockRateLimiter& unsafe_mock_rate_limiter = *mock_rate_limiter;
  InSequence s;

  EXPECT_CALL(unsafe_mock_rate_limiter, tryAcquireOne)
      .Times(burst_size)
      .WillRepeatedly(Return(true));
  RateLimiterPtr rate_limiter =
      std::make_unique<BurstingRateLimiter>(std::move(mock_rate_limiter), burst_size);

  // On the first acquisition the bursting rate limiter will have accumulated three.
  EXPECT_TRUE(rate_limiter->tryAcquireOne());
  rate_limiter->releaseOne();
  EXPECT_TRUE(rate_limiter->tryAcquireOne());
  EXPECT_TRUE(rate_limiter->tryAcquireOne());

  // Releasing one here should result in one more successfull acquisition, as the
  // BurstingRateLimiter is still releasing and not working to accumulate a new burst.
  rate_limiter->releaseOne();
  EXPECT_TRUE(rate_limiter->tryAcquireOne());
  EXPECT_TRUE(rate_limiter->tryAcquireOne());
  EXPECT_CALL(unsafe_mock_rate_limiter, tryAcquireOne).Times(1).WillOnce(Return(false));
  EXPECT_FALSE(rate_limiter->tryAcquireOne());
}

TEST_F(RateLimiterTest, ScheduledStartingRateLimiterTest) {
  Envoy::Event::SimulatedTimeSystem time_system;
  const auto schedule_delay = 10ms;
  // We test regular flow, but also the flow where the first acquisition attempt comes after the
  // scheduled delay. This should be business as usual from a functional perspective, but internally
  // this rate limiter specializes on this case to log a warning message, and we want to cover that.
  for (const bool starting_late : std::vector<bool>{false, true}) {
    const Envoy::SystemTime scheduled_starting_time = time_system.systemTime() + schedule_delay;
    std::unique_ptr<MockRateLimiter> mock_rate_limiter = std::make_unique<MockRateLimiter>();
    MockRateLimiter& unsafe_mock_rate_limiter = *mock_rate_limiter;
    InSequence s;

    EXPECT_CALL(unsafe_mock_rate_limiter, timeSource)
        .Times(AtLeast(1))
        .WillRepeatedly(ReturnRef(time_system));
    RateLimiterPtr rate_limiter = std::make_unique<ScheduledStartingRateLimiter>(
        std::move(mock_rate_limiter), scheduled_starting_time);
    EXPECT_CALL(unsafe_mock_rate_limiter, tryAcquireOne)
        .Times(AtLeast(1))
        .WillRepeatedly(Return(true));

    if (starting_late) {
      time_system.advanceTimeWait(schedule_delay);
    }

    // We should expect zero releases until it is time to start.
    while (time_system.systemTime() < scheduled_starting_time) {
      EXPECT_FALSE(rate_limiter->tryAcquireOne());
      time_system.advanceTimeWait(1ms);
    }

    // Now that is time to start, the rate limiter should propagate to the mock rate limiter.
    EXPECT_TRUE(rate_limiter->tryAcquireOne());
  }
}

TEST_F(RateLimiterTest, ScheduledStartingRateLimiterTestBadArgs) {
  Envoy::Event::SimulatedTimeSystem time_system;
  // Verify we enforce future-only scheduling.
  for (const auto& timing :
       std::vector<Envoy::SystemTime>{time_system.systemTime(), time_system.systemTime() - 10ms}) {
    std::unique_ptr<MockRateLimiter> mock_rate_limiter = std::make_unique<MockRateLimiter>();
    MockRateLimiter& unsafe_mock_rate_limiter = *mock_rate_limiter;
    EXPECT_CALL(unsafe_mock_rate_limiter, timeSource)
        .Times(AtLeast(1))
        .WillRepeatedly(ReturnRef(time_system));
    EXPECT_NO_THROW(ScheduledStartingRateLimiter(std::move(mock_rate_limiter), timing));
    // TODO(XXX): once we can, verify a warning gets logged while running the line
    // above.
  }
}

class BurstingRateLimiterIntegrationTest : public Test {
public:
  void testBurstSize(const uint64_t burst_size, const Frequency frequency) {
    Envoy::Event::SimulatedTimeSystem time_system;
    RateLimiterPtr rate_limiter = std::make_unique<BurstingRateLimiter>(
        std::make_unique<LinearRateLimiter>(time_system, frequency), burst_size);
    const auto burst_interval_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(frequency.interval() * burst_size);

    int first_burst = -1;
    for (int i = 0; i < 10000; i++) {
      uint64_t burst_acquired = 0;
      while (rate_limiter->tryAcquireOne()) {
        burst_acquired++;
      }
      if (burst_acquired) {
        first_burst = first_burst == -1 ? i : first_burst;
        EXPECT_EQ(burst_acquired, burst_size);
        EXPECT_EQ(i % (burst_interval_ms.count() - first_burst), 0);
      }
      time_system.advanceTimeWait(1ms);
    }
  }
};

TEST_F(BurstingRateLimiterIntegrationTest, BurstingLinearRateLimiterTest) {
  testBurstSize(1, 100_Hz);
  testBurstSize(2, 100_Hz);
  testBurstSize(13, 100_Hz);
  testBurstSize(100, 100_Hz);

  testBurstSize(1, 50_Hz);
  testBurstSize(2, 50_Hz);
  testBurstSize(13, 50_Hz);
  testBurstSize(100, 50_Hz);
}

TEST_F(RateLimiterTest, DistributionSamplingRateLimiterImplTest) {
  const uint64_t tries = 1000;
  auto mock_rate_limiter = std::make_unique<MockRateLimiter>();
  MockRateLimiter& unsafe_mock_rate_limiter = *mock_rate_limiter;
  Envoy::Event::SimulatedTimeSystem time_system;
  EXPECT_CALL(unsafe_mock_rate_limiter, timeSource)
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(time_system));
  auto sampler = std::make_unique<UniformRandomDistributionSamplerImpl>(1);
  EXPECT_EQ(sampler->min(), 0);
  EXPECT_EQ(sampler->max(), 1);
  RateLimiterPtr rate_limiter = std::make_unique<DistributionSamplingRateLimiterImpl>(
      std::move(sampler), std::move(mock_rate_limiter));

  EXPECT_CALL(unsafe_mock_rate_limiter, tryAcquireOne).Times(tries).WillRepeatedly(Return(true));
  // 1 in a billion chance of failure to exceed max_expected_acquisitions.
  const int max_expected_acquisitions = (tries / 2) + 30;
  EXPECT_CALL(unsafe_mock_rate_limiter, releaseOne).Times(AtMost(max_expected_acquisitions));

  int acquisitions = 0;
  // We used a 1ns upper bound. That means we can expect around 50% of acquisitions to succeed as
  // there are only two possibilities: now, or 1ns later in the future.
  for (uint64_t i = 0; i < tries; i++) {
    if (rate_limiter->tryAcquireOne()) {
      acquisitions++;
      // We test the release gets propagated to the mock rate limiter.
      // also, the release will force DelegatingRateLimiterImpl to propagate tryAcquireOne.
      rate_limiter->releaseOne();
    }
  }
  EXPECT_LT(acquisitions, max_expected_acquisitions);
}

// A rate limiter determines when acquisition is allowed, but DistributionSamplingRateLimiterImpl
// may arbitrarily delay that. We test that principle with tests that use this fixture, which
// sets up a distribution sampling rate limiter instance to encapsulate a mock rate limiter,
// relying on simulated time and a mock discrete numberic distribution sampler.
class DistributionSamplingRateLimiterTest : public RateLimiterTest {
public:
  DistributionSamplingRateLimiterTest()
      : tmp_mock_inner_rate_limiter_(std::make_unique<NiceMock<MockRateLimiter>>()),
        mock_inner_rate_limiter_(*tmp_mock_inner_rate_limiter_),
        tmp_mock_discrete_numeric_distribution_sampler_(
            std::make_unique<MockDiscreteNumericDistributionSampler>()),
        mock_discrete_numeric_distribution_sampler_(
            *tmp_mock_discrete_numeric_distribution_sampler_),
        rate_limiter_(std::make_unique<DistributionSamplingRateLimiterImpl>(
            std::move(tmp_mock_discrete_numeric_distribution_sampler_),
            std::move(tmp_mock_inner_rate_limiter_))) {
    EXPECT_CALL(mock_inner_rate_limiter_, timeSource).WillRepeatedly(ReturnRef(time_system_));
  }

  Envoy::Event::SimulatedTimeSystem time_system_;
  std::unique_ptr<NiceMock<MockRateLimiter>> tmp_mock_inner_rate_limiter_;
  MockRateLimiter& mock_inner_rate_limiter_;
  std::unique_ptr<MockDiscreteNumericDistributionSampler>
      tmp_mock_discrete_numeric_distribution_sampler_;
  MockDiscreteNumericDistributionSampler& mock_discrete_numeric_distribution_sampler_;
  RateLimiterPtr rate_limiter_;
};

TEST_F(DistributionSamplingRateLimiterTest, SingleAcquisition) {
  EXPECT_CALL(mock_inner_rate_limiter_, tryAcquireOne)
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(false));
  // The distribution first yields a 1 ns offset.
  EXPECT_CALL(mock_discrete_numeric_distribution_sampler_, getValue).WillOnce(Return(1));
  // We don't expect to be green lighted without moving time forward.
  EXPECT_FALSE(rate_limiter_->tryAcquireOne());
  time_system_.advanceTimeWait(1ns);
  EXPECT_TRUE(rate_limiter_->tryAcquireOne());
  EXPECT_FALSE(rate_limiter_->tryAcquireOne());
}

TEST_F(DistributionSamplingRateLimiterTest, QueuedAcquisition) {
  EXPECT_CALL(mock_inner_rate_limiter_, tryAcquireOne)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(false));
  // The distribution yields a 1 ns offset two times.
  EXPECT_CALL(mock_discrete_numeric_distribution_sampler_, getValue)
      .WillOnce(Return(1))
      .WillOnce(Return(1));
  // We do not expect to observe releases because we did not move time forward.
  EXPECT_FALSE(rate_limiter_->tryAcquireOne());
  EXPECT_FALSE(rate_limiter_->tryAcquireOne());
  time_system_.advanceTimeWait(1ns);
  // We moved time forward, release timings that have been queued up earlier should now be observed.
  EXPECT_TRUE(rate_limiter_->tryAcquireOne());
  EXPECT_TRUE(rate_limiter_->tryAcquireOne());
  // This should be all of it, so no further acquisitions are to be expected.
  EXPECT_FALSE(rate_limiter_->tryAcquireOne());
}

TEST_F(DistributionSamplingRateLimiterTest, ReleaseOneFunctionsWhenAcquired) {
  EXPECT_CALL(mock_inner_rate_limiter_, tryAcquireOne).WillOnce(Return(true));
  EXPECT_CALL(mock_discrete_numeric_distribution_sampler_, getValue).WillOnce(Return(0));
  EXPECT_TRUE(rate_limiter_->tryAcquireOne());
  EXPECT_CALL(mock_inner_rate_limiter_, releaseOne).Times(1);
  rate_limiter_->releaseOne();
}

// Calling releaseOne() without a prior acquisition is invavlid
TEST_F(DistributionSamplingRateLimiterTest, ReleaseOneDiesWhenNotAcquired) {
  EXPECT_DEATH(rate_limiter_->releaseOne(),
               "unexpected call to DelegatingRateLimiterImpl::releaseOne");
  EXPECT_CALL(mock_inner_rate_limiter_, tryAcquireOne).WillOnce(Return(true));
  EXPECT_CALL(mock_discrete_numeric_distribution_sampler_, getValue).WillOnce(Return(0));
  EXPECT_TRUE(rate_limiter_->tryAcquireOne());
  rate_limiter_->releaseOne();
  EXPECT_DEATH(rate_limiter_->releaseOne(),
               "unexpected call to DelegatingRateLimiterImpl::releaseOne");
}

// The DistributionSamplingRateLimiter may queues up timings for deferred release later on. Here we
// verify those are deferred release timings happen at the expected points in time. This is
// important, because the associated distribution sampler may give the
// DistributionSamplingRateLimiter random time offsets as inputs.
TEST_F(DistributionSamplingRateLimiterTest, QueuedAcquisitionCorrectReleaseOrdering) {
  // The vector below defines the sequence of timing offsets that the mock distribution sampler will
  // yield.
  std::vector<uint64_t> input_acquisition_timings_ms = {0, 0, 15000, 7, 3, 700, 2,
                                                        2, 1, 800,   4, 7, 9};
  uint64_t i = 0;
  uint64_t j = 0;
  EXPECT_CALL(mock_inner_rate_limiter_, tryAcquireOne)
      .WillRepeatedly([&i, input_acquisition_timings_ms]() {
        return ++i <= input_acquisition_timings_ms.size() ? true : false;
      });
  EXPECT_CALL(mock_discrete_numeric_distribution_sampler_, getValue)
      .Times(input_acquisition_timings_ms.size())
      .WillRepeatedly(
          [&j, input_acquisition_timings_ms]() { return input_acquisition_timings_ms[j++] * 1e6; });

  // Here we are at T0. The mock rate limiter isn't time dependent when it comes to releasing.
  // So here we iterate over the expected input acquisition timings, and the outer rate limiter
  // will buffer those that indicate an offset > 0. Zero-valued offsets ought to be released
  // immediately.
  std::vector<uint64_t> acquisition_timings;
  for (uint64_t k : input_acquisition_timings_ms) {
    if (k == 0) {
      EXPECT_TRUE(rate_limiter_->tryAcquireOne());
      acquisition_timings.push_back(0);
    } else {
      EXPECT_FALSE(rate_limiter_->tryAcquireOne());
    }
  }

  // Now we will start moving the clock, and see if the accrued deferred releases result in the
  // correct timings.
  const std::chrono::seconds duration = 15s;
  auto total_ms_elapsed = 0ms;
  const auto kClockTick = 1ms;
  do {
    while (rate_limiter_->tryAcquireOne()) {
      acquisition_timings.push_back(total_ms_elapsed.count());
    }
    time_system_.advanceTimeWait(kClockTick);
    total_ms_elapsed += kClockTick;
  } while (total_ms_elapsed <= duration);

  // The observed timings should equal the sorted offsets we had at the input.
  std::sort(input_acquisition_timings_ms.begin(), input_acquisition_timings_ms.end());
  EXPECT_EQ(acquisition_timings, input_acquisition_timings_ms);
}

class LinearRampingRateLimiterImplTest : public Test {
public:
  /**
   * @param frequency The final frequency of the ramp.
   * @param duration The test (and ramp) duration. Frequency will be 0 Hz at the start and
   * linearly increase as time moves forward, up to the specified frequency.
   * @return std::vector<uint64_t> an array containing the acquisition timings
   * in microseconds.
   */
  std::vector<int64_t> checkAcquisitionTimings(const Frequency frequency,
                                               const std::chrono::seconds duration) {
    Envoy::Event::SimulatedTimeSystem time_system;
    std::vector<int64_t> acquisition_timings;
    std::vector<int64_t> control_timings;

    LinearRampingRateLimiterImpl rate_limiter(time_system, duration, frequency);
    auto total_us_elapsed = 0us;
    const auto clock_tick = 10us;
    EXPECT_FALSE(rate_limiter.tryAcquireOne());
    do {
      if (rate_limiter.tryAcquireOne()) {
        EXPECT_FALSE(rate_limiter.tryAcquireOne());
        acquisition_timings.push_back(total_us_elapsed.count());
      }
      // We use the second law of motion to verify results: ½ * a  * t²
      // In this formula, 'a' equates to our ramp speed, and t to elapsed time.
      double t = total_us_elapsed.count() / 1e6;
      double a = (frequency.value() / (duration.count() * 1.0));
      // Finally, figure out the ground that we can expect to be covered.
      uint64_t expected_count = std::round(0.5 * a * t * t);
      if (expected_count > control_timings.size()) {
        control_timings.push_back(total_us_elapsed.count());
      }
      time_system.advanceTimeWait(clock_tick);
      total_us_elapsed += clock_tick;
    } while (total_us_elapsed <= duration);

    // For good measure, verify we saw the expected amount of acquisitions: half
    // of "frequency times duration".
    EXPECT_EQ(std::round(duration.count() * frequency.value() / 2.0), acquisition_timings.size());
    // Sanity check that we have the right number of control timings.
    EXPECT_EQ(control_timings.size(), acquisition_timings.size());
    // Verify that all timings are correct.
    for (uint64_t i = 0; i < acquisition_timings.size(); i++) {
      // We allow one clock tick of slack in timing expectations, as floating
      // point math may introduce small errors in some cases.
      // This is a test only issue: in practice we don't have a fixed microsecond-level step sizes,
      // and the rate limiter computes at nanosecond precision internally. As we want to have
      // microsecond level precision, this should be more then sufficient.
      EXPECT_NEAR(acquisition_timings[i], control_timings[i], clock_tick.count());
    }
    return acquisition_timings;
  }
};

TEST_F(RateLimiterTest, LinearRampingRateLimiterImplInvalidArgumentTest) {
  Envoy::Event::SimulatedTimeSystem time_system;
  // bad frequency
  EXPECT_THROW(LinearRampingRateLimiterImpl rate_limiter(time_system, 1s, 0_Hz);
               , NighthawkException);
  // bad ramp duration
  EXPECT_THROW(LinearRampingRateLimiterImpl rate_limiter(time_system, 0s, 1_Hz);
               , NighthawkException);
  EXPECT_THROW(LinearRampingRateLimiterImpl rate_limiter(time_system, -1s, 1_Hz);
               , NighthawkException);
}

TEST_F(LinearRampingRateLimiterImplTest, TimingVerificationTest) {
  EXPECT_EQ(checkAcquisitionTimings(5_Hz, 5s),
            std::vector<int64_t>({1000010, 1732060, 2236070, 2645760, 3000000, 3316630, 3605560,
                                  3872990, 4123110, 4358900, 4582580, 4795840, 5000000}));
  checkAcquisitionTimings(1_Hz, 3s);
  checkAcquisitionTimings(5_Hz, 3s);
  checkAcquisitionTimings(4_Hz, 2s);
  checkAcquisitionTimings(1000_Hz, 12s);
  checkAcquisitionTimings(40000_Hz, 7s);
}

TEST_F(RateLimiterTest, GraduallyOpeningRateLimiterFilterInvalidArgumentTest) {
  // Negative ramp throws.
  EXPECT_THROW(GraduallyOpeningRateLimiterFilter gorl(
                   -1s, std::make_unique<NiceMock<MockDiscreteNumericDistributionSampler>>(),
                   std::make_unique<NiceMock<MockRateLimiter>>());
               , NighthawkException);

  // zero ramp throws.
  EXPECT_THROW(GraduallyOpeningRateLimiterFilter gorl(
                   0s, std::make_unique<NiceMock<MockDiscreteNumericDistributionSampler>>(),
                   std::make_unique<NiceMock<MockRateLimiter>>());
               , NighthawkException);

  // Pass in a badly configured distribution sampler.
  auto bad_distribution_sampler = std::make_unique<MockDiscreteNumericDistributionSampler>();
  EXPECT_CALL(*bad_distribution_sampler, min).Times(1).WillOnce(Return(0));
  EXPECT_THROW(
      GraduallyOpeningRateLimiterFilter gorl(1s, std::move(bad_distribution_sampler),
                                             std::make_unique<NiceMock<MockRateLimiter>>());
      , NighthawkException);

  bad_distribution_sampler = std::make_unique<MockDiscreteNumericDistributionSampler>();
  // Correct min, but now introduce a bad max.
  EXPECT_CALL(*bad_distribution_sampler, min).Times(1).WillOnce(Return(1));
  EXPECT_CALL(*bad_distribution_sampler, max).Times(1).WillOnce(Return(99));
  EXPECT_THROW(
      GraduallyOpeningRateLimiterFilter gorl(1s, std::move(bad_distribution_sampler),
                                             std::make_unique<NiceMock<MockRateLimiter>>());
      , NighthawkException);
}

class GraduallyOpeningRateLimiterFilterTest : public Test {
public:
  std::vector<int64_t> getAcquisitionTimings(const Frequency frequency,
                                             const std::chrono::seconds duration) {
    Envoy::Event::SimulatedTimeSystem time_system;
    std::vector<int64_t> acquisition_timings;
    auto* unsafe_discrete_numeric_distribution_sampler =
        new MockDiscreteNumericDistributionSampler();
    const uint64_t dist_min = 1;
    const uint64_t dist_max = 1000000;
    EXPECT_CALL(*unsafe_discrete_numeric_distribution_sampler, getValue)
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([]() { return (dist_min + dist_max) / 2; }));
    EXPECT_CALL(*unsafe_discrete_numeric_distribution_sampler, min)
        .Times(1)
        .WillOnce(Return(dist_min));
    EXPECT_CALL(*unsafe_discrete_numeric_distribution_sampler, max)
        .Times(AtLeast(1))
        .WillRepeatedly(Return(dist_max));
    RateLimiterPtr rate_limiter = std::make_unique<GraduallyOpeningRateLimiterFilter>(
        duration,
        std::unique_ptr<DiscreteNumericDistributionSampler>(
            unsafe_discrete_numeric_distribution_sampler),
        std::make_unique<LinearRateLimiter>(time_system, frequency));
    auto total_ms_elapsed = 0ms;
    auto clock_tick = 1ms;
    EXPECT_FALSE(rate_limiter->tryAcquireOne());

    do {
      if (rate_limiter->tryAcquireOne()) {
        acquisition_timings.push_back(total_ms_elapsed.count());
        EXPECT_FALSE(rate_limiter->tryAcquireOne());
      }
      time_system.advanceTimeWait(clock_tick);
      total_ms_elapsed += clock_tick;
    } while (total_ms_elapsed <= duration);

    EXPECT_FALSE(rate_limiter->tryAcquireOne());
    time_system.advanceTimeWait(1s);
    // Verify that after the rampup the expected constant pacing is maintained.
    // Calls should be forwarded to the regular linear rate limiter algorithm with its
    // corrective behavior so we can expect to acquire a series with that.
    for (uint64_t i = 0; i < frequency.value(); i++) {
      EXPECT_TRUE(rate_limiter->tryAcquireOne());
    }
    // Verify we acquired everything.
    EXPECT_FALSE(rate_limiter->tryAcquireOne());
    // Verify releaseOne works.
    rate_limiter->releaseOne();
    EXPECT_TRUE(rate_limiter->tryAcquireOne());
    EXPECT_FALSE(rate_limiter->tryAcquireOne());
    return acquisition_timings;
  }
};

TEST_F(GraduallyOpeningRateLimiterFilterTest, TimingVerificationTest) {
  EXPECT_EQ(getAcquisitionTimings(50_Hz, 1s),
            std::vector<int64_t>({510, 530, 550, 570, 590, 610, 630, 650, 670, 690, 710, 730, 750,
                                  770, 790, 810, 830, 850, 870, 890, 910, 930, 950, 970, 990}));
}

class ZipfRateLimiterImplTest : public Test {};

TEST_F(ZipfRateLimiterImplTest, TimingVerificationTest) {
  Envoy::Event::SimulatedTimeSystem time_system;
  const double q = 2.0;
  const double v = 1.0;
  auto rate_limiter = std::make_unique<ZipfRateLimiterImpl>(
      std::make_unique<LinearRateLimiter>(time_system, 10_Hz), q, v,
      ZipfRateLimiterImpl::ZipfBehavior::ZIPF_PSEUDO_RANDOM);
  const std::chrono::seconds duration = 15s;
  std::vector<int64_t> acquisition_timings;
  auto total_ms_elapsed = 0ms;
  auto clock_tick = 1ms;

  do {
    if (rate_limiter->tryAcquireOne()) {
      acquisition_timings.push_back(total_ms_elapsed.count());
    }
    time_system.advanceTimeWait(clock_tick);
    total_ms_elapsed += clock_tick;
  } while (total_ms_elapsed <= duration);
  EXPECT_EQ(acquisition_timings,
            std::vector<int64_t>({450,   750,   1250,  2350,  2850,  3850,  4150,  4350,  4450,
                                  5750,  5950,  6350,  7850,  8350,  8550,  9850,  10150, 10450,
                                  10550, 11950, 12250, 12550, 13250, 13550, 13650, 13750, 13850}));
}

TEST_F(ZipfRateLimiterImplTest, BadArgumentsTest) {
  // Zipf preconditions are q > 1, v > 0, verify we guard appropriately.
  std::list<std::tuple<double, double>> bad_q_v_pairs{
      {1.0, 1.0} /*borderline bad q*/,
      {1.1, 0.0} /*borderline bad v*/,
      {1.0, 0.0} /*borderline bad both*/,
      {0.9, 1.0},
      {1.1, -1.0},
      {-1, 1.0},
  };

  for (const auto& pair : bad_q_v_pairs) {
    EXPECT_THROW(ZipfRateLimiterImpl rate_limiter(std::make_unique<NiceMock<MockRateLimiter>>(),
                                                  std::get<0>(pair), std::get<1>(pair)),
                 NighthawkException);
  }
}

} // namespace Nighthawk
