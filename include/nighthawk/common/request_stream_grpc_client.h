#pragma once

#include <memory>

#include "nighthawk/common/request.h"

namespace Nighthawk {

/**
 * Interface for a gRPC client used to pull request data from a gRPC service.
 */
class RequestStreamGrpcClient {
public:
  virtual ~RequestStreamGrpcClient() = default;
  /**
   * Performs initial stream establishment as well as requests the initial set of to-be-replayed
   * requests.
   */
  virtual void start() PURE;
  /**
   * @return RequestPtr in FIFO order for replay. may equal nullptr if the queue is empty.
   */
  virtual RequestPtr maybeDequeue() PURE;
  /**
   * @return true iff the stream status is known to be either functional or disfunctional.
   */
  virtual bool streamStatusKnown() const PURE;
};

using RequestStreamGrpcClientPtr = std::unique_ptr<RequestStreamGrpcClient>;

} // namespace Nighthawk