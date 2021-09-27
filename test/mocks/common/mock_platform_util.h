#pragma once

#include "nighthawk/common/platform_util.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockPlatformUtil : public PlatformUtil {
public:
  MockPlatformUtil();

  MOCK_METHOD(void, yieldCurrentThread, (), (const, override));
  MOCK_METHOD(void, sleep, (std::chrono::microseconds), (const, override));
};

} // namespace Nighthawk