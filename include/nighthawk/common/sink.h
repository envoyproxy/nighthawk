
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
   * Stores a fragement of the execution results.
   *
   * @param response
   */
  virtual absl::Status
  StoreExecutionResultPiece(const ::nighthawk::client::ExecutionResponse& response) const PURE;
  virtual const absl::StatusOr<std::vector<::nighthawk::client::ExecutionResponse>>
  LoadExecutionResult(absl::string_view id) const PURE;
};

} // namespace Nighthawk