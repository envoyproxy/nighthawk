#pragma once

#include "nighthawk/common/factories.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockRequestSourceFactory : public RequestSourceFactory {
public:
  MockRequestSourceFactory();
  MOCK_CONST_METHOD1(create,
                     RequestSourcePtr(const RequestSourceConstructorInterface& request_source_constructor));
};

} // namespace Nighthawk