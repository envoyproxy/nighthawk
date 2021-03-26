#pragma once

#include "nighthawk/sink/sink.h"

#include "absl/container/flat_hash_map.h"

namespace Nighthawk {

/**
 * Filesystem based implementation of Sink. Uses /tmp/nh/{execution_id}/ to store and load
 * data.
 */
class FileSinkImpl : public Sink {
public:
  absl::Status
  StoreExecutionResultPiece(const nighthawk::client::ExecutionResponse& response) override;
  absl::StatusOr<std::vector<nighthawk::client::ExecutionResponse>>
  LoadExecutionResult(absl::string_view id) const override;
};

/**
 * Memory based implementation of Sink.
 */
class InMemorySinkImpl : public Sink {
public:
  absl::Status
  StoreExecutionResultPiece(const nighthawk::client::ExecutionResponse& response) override;
  absl::StatusOr<std::vector<nighthawk::client::ExecutionResponse>>
  LoadExecutionResult(absl::string_view id) const override;

private:
  absl::flat_hash_map<std::string, std::vector<nighthawk::client::ExecutionResponse>> map_;
};

} // namespace Nighthawk
