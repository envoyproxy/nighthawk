#pragma once

#include "nighthawk/adaptive_load/session_spec_proto_helper.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockAdaptiveLoadSessionSpecProtoHelper : public AdaptiveLoadSessionSpecProtoHelper {
public:
  MockAdaptiveLoadSessionSpecProtoHelper();

  MOCK_CONST_METHOD1(SetSessionSpecDefaults,
                     nighthawk::adaptive_load::AdaptiveLoadSessionSpec(
                         const nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec));
  MOCK_CONST_METHOD1(CheckSessionSpec,
                     absl::Status(const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec));
};

} // namespace Nighthawk
