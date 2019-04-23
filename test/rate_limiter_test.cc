#include <chrono>

#include "gtest/gtest.h"

#include "test/test_common/simulated_time_system.h"

#include "nighthawk/common/exception.h"

#include "common/frequency.h"
#include "common/rate_limiter_impl.h"

#include "test/mocks.h"

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
  ASSERT_DEATH(LinearRateLimiter rate_limiter(time_system, 0_Hz), "Frequency must be > 0");
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

    EXPECT_FALSE(rate_limiter->tryAcquireOne());
    time_system.sleep(burst_interval_ms);
    for (uint64_t i = 0; i < burst_size; i++) {
      EXPECT_TRUE(rate_limiter->tryAcquireOne());
    }
    EXPECT_FALSE(rate_limiter->tryAcquireOne());
    time_system.sleep(burst_interval_ms / 2);
    EXPECT_FALSE(rate_limiter->tryAcquireOne());
    time_system.sleep(burst_interval_ms);
    for (uint64_t i = 0; i < burst_size; i++) {
      EXPECT_TRUE(rate_limiter->tryAcquireOne());
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

} // namespace Nighthawk
