#pragma once

#include <functional>

#include "envoy/http/header_map.h"

namespace Nighthawk {

using HeaderMapPtr = std::shared_ptr<const Envoy::Http::HeaderMap>;

class Request {
public:
  virtual ~Request() = default;
  virtual HeaderMapPtr header() const PURE;
  // TODO(oschaaf): expectations
};

using RequestPtr = std::unique_ptr<Request>;

} // namespace Nighthawk
