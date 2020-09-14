#pragma once

#include "nighthawk/common/nighthawk_service_client.h"

#include "gmock/gmock.h"

namespace Nighthawk {

/**
 * A mock NighthawkServiceClient that returns an empty response by default.
 *
 * Typical usage:
 *
 *   NiceMock<MockNighthawkServiceClient> mock_nighthawk_service_client;
 *   nighthawk::client::ExecutionResponse nighthawk_response;
 *   EXPECT_CALL(mock_nighthawk_service_client, PerformNighthawkBenchmark(_, _))
 *       .WillRepeatedly(Return(nighthawk_response));
 */
class MockNighthawkServiceClient : public NighthawkServiceClient {
public:
  /**
   * Empty constructor.
   */
  MockNighthawkServiceClient();

  MOCK_CONST_METHOD2(PerformNighthawkBenchmark,
                     absl::StatusOr<nighthawk::client::ExecutionResponse>(
                         nighthawk::client::NighthawkService::StubInterface* stub,
                         const nighthawk::client::CommandLineOptions& options));
};

} // namespace Nighthawk
