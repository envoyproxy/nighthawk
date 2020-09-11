#pragma once

#include "nighthawk/common/nighthawk_service_client.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockNighthawkServiceClient : public NighthawkServiceClient {
public:
  MockNighthawkServiceClient();

  MOCK_CONST_METHOD2(PerformNighthawkBenchmark,
                     absl::StatusOr<nighthawk::client::ExecutionResponse>(
                         nighthawk::client::NighthawkService::StubInterface* stub,
                         const nighthawk::client::CommandLineOptions& options));
};

} // namespace Nighthawk
