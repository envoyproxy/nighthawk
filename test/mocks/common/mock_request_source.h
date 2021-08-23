#pragma once

#include "nighthawk/common/request_source.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockRequestSource : public RequestSource {
public:
  MockRequestSource();
  MOCK_METHOD(RequestGenerator, get, ());
  MOCK_METHOD(void, initOnThread, ());
  MOCK_METHOD(void, destroyOnThread, ());
};

} // namespace Nighthawk
