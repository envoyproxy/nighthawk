#pragma once

#include <memory>

#include "nighthawk/common/header_source.h"

namespace Nighthawk {
class ReplayGrpcClient {
public:
  virtual ~ReplayGrpcClient() = default;
  virtual bool establishNewStream() PURE;
  virtual HeaderMapPtr maybeDequeue() PURE;
};

using ReplayGrpcClientPtr = std::unique_ptr<ReplayGrpcClient>;

} // namespace Nighthawk