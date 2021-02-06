#pragma once

#include "nighthawk/common/sink.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockSink : public Sink {
public:
  MockSink();
  MOCK_CONST_METHOD1(StoreExecutionResultPiece,
                     absl::Status(const ::nighthawk::client::ExecutionResponse&));
  MOCK_CONST_METHOD1(
      LoadExecutionResult,
      const absl::StatusOr<std::vector<::nighthawk::client::ExecutionResponse>>(absl::string_view));
};

} // namespace Nighthawk