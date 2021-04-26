#pragma once

#include "nighthawk/sink/sink.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockSink : public Sink {
public:
  MockSink();
  MOCK_METHOD(absl::Status, StoreExecutionResultPiece,
              (const nighthawk::client::ExecutionResponse&));
  MOCK_METHOD(absl::StatusOr<std::vector<nighthawk::client::ExecutionResponse>>,
              LoadExecutionResult, (absl::string_view), (const));
};

} // namespace Nighthawk
