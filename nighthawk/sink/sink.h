
#pragma once

#include <vector>

#include "envoy/common/pure.h"

#include "external/envoy/source/common/common/statusor.h"

#include "api/client/service.grpc.pb.h"

#include "absl/strings/string_view.h"

namespace Nighthawk {

/**
 * Abstract Sink interface.
 */
class Sink {
public:
  virtual ~Sink() = default;

  /**
   * Store an ExecutionResponse instance. Can be called multiple times for the same execution_id to
   * persist multiple fragments that together will represent results belonging to a single
   * execution.
   *
   * @param response Specify an ExecutionResponse instance that should be persisted. The
   * ExecutionResponse must have its execution_id set.
   * @return absl::Status Indicates if the operation succeeded or not.
   */
  virtual absl::Status
  StoreExecutionResultPiece(const nighthawk::client::ExecutionResponse& response) PURE;

  /**
   * Attempt to load a vector of ExecutionResponse instances associated to an execution id.
   *
   * @param execution_id Specify an execution_id that the desired set of ExecutionResponse
   * instances are tagged with.
   * @return absl::StatusOr<std::vector<nighthawk::client::ExecutionResponse>>.
   * When no fragments are found for the provided execution id, status kNotFound is returned.
   */
  virtual absl::StatusOr<std::vector<nighthawk::client::ExecutionResponse>>
  LoadExecutionResult(absl::string_view execution_id) const PURE;
};

} // namespace Nighthawk