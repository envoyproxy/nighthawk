#pragma once

#include <memory>

#include "nighthawk/common/request_source.h"

namespace Nighthawk {
class HeaderStreamGrpcClient {
public:
  virtual ~HeaderStreamGrpcClient() = default;
  virtual void start() PURE;
  virtual RequestPtr maybeDequeue() PURE;
  virtual bool stream_status_known() const PURE;
};

using HeaderStreamGrpcClientPtr = std::unique_ptr<HeaderStreamGrpcClient>;

} // namespace Nighthawk