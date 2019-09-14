#pragma once

#include <functional>

#include "envoy/http/header_map.h"

namespace Nighthawk {

using HeaderMapPtr = std::shared_ptr<const Envoy::Http::HeaderMap>;
using HeaderGenerator = std::function<HeaderMapPtr()>;

class HeaderSource {
public:
  virtual ~HeaderSource() = default;
  virtual HeaderGenerator get() PURE;
};

using HeaderSourcePtr = std::unique_ptr<HeaderSource>;

} // namespace Nighthawk
