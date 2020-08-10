#pragma once

#include "nighthawk/common/platform_util.h"

#include "gmock/gmock.h"

namespace nighthawk {

class MockPlatformUtil : public PlatformUtil {
public:
  MockPlatformUtil();

  MOCK_CONST_METHOD0(yieldCurrentThread, void());
  MOCK_CONST_METHOD1(sleep, void(std::chrono::microseconds));
};

} // namespace nighthawk