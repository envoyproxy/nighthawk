#pragma once

#include "nighthawk/common/rate_limiter.h"
#include "nighthawk/common/sequencer.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockSequencer : public Sequencer {
public:
  MockSequencer();

  MOCK_METHOD(void, start, ());
  MOCK_METHOD(void, waitForCompletion, ());
  MOCK_METHOD(double, completionsPerSecond, (), (const));
  MOCK_METHOD(std::chrono::nanoseconds, executionDuration, (), (const));
  MOCK_METHOD(StatisticPtrMap, statistics, (), (const));
  MOCK_METHOD(void, cancel, ());
  MOCK_METHOD(RateLimiter&, rate_limiter, (), (const));
};

} // namespace Nighthawk