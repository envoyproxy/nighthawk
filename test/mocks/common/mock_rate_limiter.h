#pragma once

#include "nighthawk/common/rate_limiter.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockRateLimiter : public RateLimiter {
public:
  MockRateLimiter();

  MOCK_METHOD0(tryAcquireOne, bool());
  MOCK_METHOD0(releaseOne, void());
  MOCK_METHOD0(timeSource, Envoy::TimeSource&());
  MOCK_METHOD0(elapsed, std::chrono::nanoseconds());
  MOCK_CONST_METHOD0(firstAcquisitionTime, absl::optional<Envoy::SystemTime>());
};

class MockDiscreteNumericDistributionSampler : public DiscreteNumericDistributionSampler {
public:
  MockDiscreteNumericDistributionSampler();
  MOCK_METHOD0(getValue, uint64_t());
  MOCK_CONST_METHOD0(min, uint64_t());
  MOCK_CONST_METHOD0(max, uint64_t());
};

} // namespace Nighthawk
