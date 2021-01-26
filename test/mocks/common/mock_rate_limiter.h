#pragma once

#include "nighthawk/common/rate_limiter.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockRateLimiter : public RateLimiter {
public:
  MockRateLimiter();

  MOCK_METHOD(bool, tryAcquireOne, ());
  MOCK_METHOD(void, releaseOne, ());
  MOCK_METHOD(Envoy::TimeSource&, timeSource, ());
  MOCK_METHOD(std::chrono::nanoseconds, elapsed, ());
  MOCK_METHOD(absl::optional<Envoy::SystemTime>, firstAcquisitionTime, (), (const));
};

class MockDiscreteNumericDistributionSampler : public DiscreteNumericDistributionSampler {
public:
  MockDiscreteNumericDistributionSampler();
  MOCK_METHOD(uint64_t, getValue, ());
  MOCK_METHOD(uint64_t, min, (), (const));
  MOCK_METHOD(uint64_t, max, (), (const));
};

} // namespace Nighthawk
