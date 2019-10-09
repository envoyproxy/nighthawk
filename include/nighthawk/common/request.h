#pragma once

#include <functional>

#include "envoy/http/header_map.h"

#include "nighthawk/common/request.h"

namespace Nighthawk {

using HeaderMapPtr = std::shared_ptr<const Envoy::Http::HeaderMap>;

class Request {
public:
  virtual ~Request() = default;
  virtual HeaderMapPtr header() const PURE;
};

using RequestPtr = std::unique_ptr<Request>;

} // namespace Nighthawk
