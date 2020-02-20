#pragma once

#include "nighthawk/common/request_source.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockRequestSource : public RequestSource {
public:
  MockRequestSource();
  MOCK_METHOD0(get, RequestGenerator());
  MOCK_METHOD0(initOnThread, void());
};

} // namespace Nighthawk