#pragma once

#include "nighthawk/common/sink.h"

namespace Nighthawk {

class FileSinkImpl : public Sink {
public:
  absl::Status
  StoreExecutionResultPiece(const ::nighthawk::client::ExecutionResponse& response) const override;
  const absl::StatusOr<std::vector<::nighthawk::client::ExecutionResponse>>
  LoadExecutionResult(absl::string_view id) const override;
};

} // namespace Nighthawk
