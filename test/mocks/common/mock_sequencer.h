#pragma once

#include "nighthawk/common/rate_limiter.h"
#include "nighthawk/common/sequencer.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockSequencer : public Sequencer {
public:
  MockSequencer();

  MOCK_METHOD0(start, void());
  MOCK_METHOD0(waitForCompletion, void());
  MOCK_CONST_METHOD0(completionsPerSecond, double());
  MOCK_CONST_METHOD0(executionDuration, std::chrono::nanoseconds());
  MOCK_CONST_METHOD0(statistics, StatisticPtrMap());
  MOCK_METHOD0(cancel, void());
  MOCK_CONST_METHOD0(rate_limiter, RateLimiter&());
};

} // namespace Nighthawk