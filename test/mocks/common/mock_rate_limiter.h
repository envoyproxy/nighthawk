#pragma once

#include "nighthawk/common/rate_limiter.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockRateLimiter : public RateLimiter {
public:
  MockRateLimiter();

  MOCK_METHOD(bool, tryAcquireOne, (), (override));
  MOCK_METHOD(void, releaseOne, (), (override));
  MOCK_METHOD(Envoy::TimeSource&, timeSource, (), (override));
  MOCK_METHOD(std::chrono::nanoseconds, elapsed, (), (override));
  MOCK_METHOD(absl::optional<Envoy::SystemTime>, firstAcquisitionTime, (),
              (const, override));
};

class MockDiscreteNumericDistributionSampler : public DiscreteNumericDistributionSampler {
public:
  MockDiscreteNumericDistributionSampler();
  MOCK_METHOD(uint64_t, getValue, (), (override));
  MOCK_METHOD(uint64_t, min, (), (const, override));
  MOCK_METHOD(uint64_t, max, (), (const, override));
};

} // namespace Nighthawk
