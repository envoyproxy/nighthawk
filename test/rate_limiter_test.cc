#include <chrono>

#include "nighthawk/common/exception.h"

#include "external/envoy/test/test_common/simulated_time_system.h"

#include "common/frequency.h"
#include "common/rate_limiter_impl.h"

#include "test/mocks.h"

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

  time_system.sleep(100ms);
  EXPECT_TRUE(rate_limiter.tryAcquireOne());
  EXPECT_FALSE(rate_limiter.tryAcquireOne());

  time_system.sleep(1s);
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

class BurstingRateLimiterIntegrationTest : public Test {
public:
  void testBurstSize(const uint64_t burst_size, const Frequency frequency) {
    Envoy::Event::SimulatedTimeSystem time_system;
    RateLimiterPtr rate_limiter = std::make_unique<BurstingRateLimiter>(
        std::make_unique<LinearRateLimiter>(time_system, frequency), burst_size);
    const auto burst_interval_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(frequency.interval() * burst_size);

    for (uint64_t i = 0; i < 10000; i++) {
      uint64_t burst_acquired = 0;
      while (rate_limiter->tryAcquireOne()) {
        burst_acquired++;
      }
      if (burst_acquired) {
        EXPECT_EQ(burst_acquired, burst_size);
        EXPECT_EQ(i % burst_interval_ms.count(), 0);
      }
      time_system.sleep(1ms);
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
  RateLimiterPtr rate_limiter = std::make_unique<DistributionSamplingRateLimiterImpl>(
      std::make_unique<UniformRandomDistributionSamplerImpl>(1), std::move(mock_rate_limiter));

  EXPECT_CALL(unsafe_mock_rate_limiter, tryAcquireOne).Times(tries).WillRepeatedly(Return(true));
  EXPECT_CALL(unsafe_mock_rate_limiter, releaseOne).Times(tries);

  int acquisitions = 0;
  // We used a 1ns upper bound. That means we can expect around 50% of acquisitions to succeed as
  // there are only two possibilities: now, or 1ns later in the future.
  for (uint64_t i = 0; i < tries; i++) {
    if (rate_limiter->tryAcquireOne()) {
      acquisitions++;
    }
    // We test the release gets propagated to the mock rate limiter.
    // also, the release will force DelegatingRateLimiterImpl to propagate tryAcquireOne.
    rate_limiter->releaseOne();
  }
  // 1 in a billion chance of failure.
  EXPECT_LT(acquisitions, (tries / 2) + 30);
}

// A rate limiter determines when acquisition is allowed, but DistributionSamplingRateLimiterImpl
// may arbitrarily delay that. We test that principle here.
TEST_F(RateLimiterTest, DistributionSamplingRateLimiterImplSchedulingTest) {
  auto mock_rate_limiter = std::make_unique<NiceMock<MockRateLimiter>>();
  MockRateLimiter& unsafe_mock_rate_limiter = *mock_rate_limiter;
  Envoy::Event::SimulatedTimeSystem time_system;
  auto* unsafe_discrete_numeric_distribution_sampler = new MockDiscreteNumericDistributionSampler();
  RateLimiterPtr rate_limiter = std::make_unique<DistributionSamplingRateLimiterImpl>(
      std::unique_ptr<DiscreteNumericDistributionSampler>(
          unsafe_discrete_numeric_distribution_sampler),
      std::move(mock_rate_limiter));
  EXPECT_CALL(unsafe_mock_rate_limiter, timeSource)
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(time_system));

  EXPECT_CALL(unsafe_mock_rate_limiter, tryAcquireOne)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(unsafe_mock_rate_limiter, releaseOne).Times(1);
  EXPECT_CALL(*unsafe_discrete_numeric_distribution_sampler, getValue)
      .Times(3)
      .WillOnce(Return(1))
      .WillOnce(Return(0))
      .WillOnce(Return(1));

  // The distribution first yields a 1 ns offset. So we don't expect to be green lighted.
  EXPECT_FALSE(rate_limiter->tryAcquireOne());
  time_system.sleep(1ns);
  EXPECT_TRUE(rate_limiter->tryAcquireOne());
  // We expect releaseOne to be propagated.
  rate_limiter->releaseOne();
  // The distribution will yield an offset of 0ns, we expect success.
  EXPECT_TRUE(rate_limiter->tryAcquireOne());

  // We don't sleep, and the distribution will yield a 1ns offset. No green light.
  EXPECT_FALSE(rate_limiter->tryAcquireOne());
  time_system.sleep(1ns);
  EXPECT_TRUE(rate_limiter->tryAcquireOne());
}

// TODO(oschaaf): once we have hr sleep, test at a higher res.
class LinearRampingRateLimiterImplTest : public Test {
public:
  std::vector<int64_t> getAcquisitionTimings(const Frequency frequency,
                                             const std::chrono::seconds duration) {
    Envoy::Event::SimulatedTimeSystem time_system;
    std::vector<int64_t> aquisition_timings;
    LinearRampingRateLimiterImpl rate_limiter(time_system, duration, frequency);
    auto total_ms_elapsed = 0ms;
    const auto clock_tick = 1ms;
    auto last_acquisition_timestamp = 0ms;
    EXPECT_FALSE(rate_limiter.tryAcquireOne());
    do {
      if (rate_limiter.tryAcquireOne()) {
        // EXPECT_FALSE(rate_limiter.tryAcquireOne());
        aquisition_timings.push_back(total_ms_elapsed.count());
        last_acquisition_timestamp = total_ms_elapsed;
      }
      time_system.sleep(clock_tick);
      total_ms_elapsed += clock_tick;
    } while (total_ms_elapsed <= duration);
    return aquisition_timings;
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
  EXPECT_EQ(getAcquisitionTimings(5_Hz, 5s),
            std::vector<int64_t>(
                {1000, 1733, 2237, 2646, 3000, 3317, 3606, 3873, 4124, 4359, 4583, 4796, 5000}));
  EXPECT_EQ(getAcquisitionTimings(4_Hz, 2s), std::vector<int64_t>({708, 1225, 1582, 1871}));
}

class GraduallyOpeningRateLimiterFilterTest : public Test {
public:
  std::vector<int64_t> getAcquisitionTimings(const Frequency frequency,
                                             const std::chrono::seconds duration) {
    Envoy::Event::SimulatedTimeSystem time_system;
    std::vector<int64_t> aquisition_timings;
    auto* unsafe_discrete_numeric_distribution_sampler =
        new MockDiscreteNumericDistributionSampler();
    std::mt19937_64 mt(1243);
    std::uniform_int_distribution<uint64_t> dist(1, 1000000);
    EXPECT_CALL(*unsafe_discrete_numeric_distribution_sampler, getValue)
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([&dist, &mt]() { return dist(mt); }));
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
        aquisition_timings.push_back(total_ms_elapsed.count());
        EXPECT_FALSE(rate_limiter->tryAcquireOne());
      }
      time_system.sleep(clock_tick);
      total_ms_elapsed += clock_tick;
    } while (total_ms_elapsed <= duration);

    EXPECT_FALSE(rate_limiter->tryAcquireOne());
    time_system.sleep(1s);
    // Verify that after the rampup the expected constant pacing is maintained.
    // Calls should be forwarded to the regular linear rate limiter algorithm with its
    // corrective behavior so we can expect to acquire a series with that.
    for (uint64_t i = 0; i < frequency.value(); i++) {
      EXPECT_TRUE(rate_limiter->tryAcquireOne());
    }
    // Verify we acquired everything.
    EXPECT_FALSE(rate_limiter->tryAcquireOne());
    return aquisition_timings;
  }
};

TEST_F(GraduallyOpeningRateLimiterFilterTest, TimingVerificationTest) {
  EXPECT_EQ(getAcquisitionTimings(50_Hz, 1s),
            std::vector<int64_t>({120, 320, 380, 560, 580, 600, 620, 640, 660, 680, 700, 740,
                                  760, 780, 840, 860, 880, 900, 920, 940, 960, 980, 1000}));
}

class ZipfRateLimiterImplTest : public Test {};

TEST_F(ZipfRateLimiterImplTest, TimingVerificationTest) {
  // TODO(oschaaf): test once we have the real thing
}

} // namespace Nighthawk
