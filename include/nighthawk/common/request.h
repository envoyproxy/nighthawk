#pragma once

#include <functional>

#include "envoy/http/header_map.h"

namespace Nighthawk {

using HeaderMapPtr = std::shared_ptr<const Envoy::Http::RequestHeaderMap>;

/**
 * Defines the specifics of requests to be send by the load generator, as well as
 * may hold request-level expectations.
 */
class Request {
public:
  virtual ~Request() = default;
  virtual HeaderMapPtr header() const PURE;
  // TODO(oschaaf): expectations
};

using RequestPtr = std::unique_ptr<Request>;

} // namespace Nighthawk
