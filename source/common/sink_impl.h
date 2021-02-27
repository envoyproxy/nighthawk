#pragma once

#include "nighthawk/common/sink.h"

namespace Nighthawk {

/**
 * Filesystem based implementation of Sink. Uses /tmp/nh/{execution_id}/ to store and load
 * data.
 */
class FileSinkImpl : public Sink {
public:
  absl::Status
  StoreExecutionResultPiece(const ::nighthawk::client::ExecutionResponse& response) const override;
  absl::StatusOr<std::vector<::nighthawk::client::ExecutionResponse>>
  LoadExecutionResult(absl::string_view id) const override;
};

} // namespace Nighthawk
