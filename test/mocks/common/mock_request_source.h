#pragma once

#include "nighthawk/common/request_source.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockRequestSource : public RequestSource {
public:
  MockRequestSource();
  MOCK_METHOD(RequestGenerator, get, (), (override));
  MOCK_METHOD(void, initOnThread, (), (override));
  MOCK_METHOD(void, destroyOnThread, (), (override));
};

} // namespace Nighthawk
