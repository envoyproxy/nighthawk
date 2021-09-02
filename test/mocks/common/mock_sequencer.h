#pragma once

#include "nighthawk/common/rate_limiter.h"
#include "nighthawk/common/sequencer.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockSequencer : public Sequencer {
public:
  MockSequencer();

  MOCK_METHOD(void, start, (), (override));
  MOCK_METHOD(void, waitForCompletion, (), (override));
  MOCK_METHOD(double, completionsPerSecond, (), (const, override));
  MOCK_METHOD(std::chrono::nanoseconds, executionDuration, (),
              (const, override));
  MOCK_METHOD(StatisticPtrMap, statistics, (), (const, override));
  MOCK_METHOD(void, cancel, ());
  MOCK_METHOD(RateLimiter&, rate_limiter, (), (const, override));
};

} // namespace Nighthawk
