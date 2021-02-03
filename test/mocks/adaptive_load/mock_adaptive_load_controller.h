#pragma once

#include "nighthawk/adaptive_load/adaptive_load_controller.h"

#include "gmock/gmock.h"

namespace Nighthawk {

/**
 * A mock AdaptiveLoadController that returns empty values or success from all methods
 * by default.
 *
 *
 * Typical usage:
 *
 *   NiceMock<MockAdaptiveLoadController> mock_controller;
 *   EXPECT_CALL(mock_controller, PerformAdaptiveLoadSession(_))
 *       .WillOnce(Return(AdaptiveLoadSessionOutput()));
 */
class MockAdaptiveLoadController : public AdaptiveLoadController {
public:
  /**
   * Empty constructor.
   */
  MockAdaptiveLoadController();

  MOCK_METHOD(absl::StatusOr<nighthawk::adaptive_load::AdaptiveLoadSessionOutput>,
              PerformAdaptiveLoadSession,
              (nighthawk::client::NighthawkService::StubInterface * nighthawk_service_stub,
               const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec));
};

} // namespace Nighthawk
