#pragma once

#include "nighthawk/adaptive_load/session_spec_proto_helper.h"

#include "gmock/gmock.h"

namespace Nighthawk {

/**
 * A mock AdaptiveLoadSessionSpecProtoHelper that returns empty values or success from all methods
 * by default.
 *
 * In particular, SetSessionSpecDefaults does not pass its input value through to its output;
 * regardless of the output, it returns an empty proto, unless explicitly configured (see below). If
 * you don't need to inspect calls to the spec proto helper, it may be easier to use the real
 * AdaptiveLoadSessionSpecProtoHelperImpl in tests instead.
 *
 * Typical usage:
 *
 *   NiceMock<MockAdaptiveLoadSessionSpecProtoHelper> mock_spec_proto_helper;
 *   EXPECT_CALL(mock_spec_proto_helper, CheckSessionSpec(_))
 *       .WillOnce(Return(absl::OkStatus()));
 *   AdaptiveLoadSessionSpec spec;
 *   // Set spec fields here, including providing all defaults yourself.
 *   EXPECT_CALL(mock_spec_proto_helper, SetSessionSpecDefaults(_))
 *       .WillOnce(Return(spec));
 */
class MockAdaptiveLoadSessionSpecProtoHelper : public AdaptiveLoadSessionSpecProtoHelper {
public:
  /**
   * Empty constructor.
   */
  MockAdaptiveLoadSessionSpecProtoHelper();

  MOCK_METHOD(nighthawk::adaptive_load::AdaptiveLoadSessionSpec, SetSessionSpecDefaults,
              (const nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec), (const, override));
  MOCK_METHOD(absl::Status, CheckSessionSpec,
              (const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec), (const, override));
};

} // namespace Nighthawk
