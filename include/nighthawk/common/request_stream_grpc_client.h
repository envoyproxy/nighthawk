#pragma once

#include <memory>

#include "nighthawk/common/request.h"

namespace Nighthawk {
class RequestStreamGrpcClient {
public:
  virtual ~RequestStreamGrpcClient() = default;
  virtual void start() PURE;
  virtual RequestPtr maybeDequeue() PURE;
  virtual bool stream_status_known() const PURE;
};

using RequestStreamGrpcClientPtr = std::unique_ptr<RequestStreamGrpcClient>;

} // namespace Nighthawk