#pragma once

#include <functional>

#include "envoy/http/header_map.h"

namespace Nighthawk {

using HeaderMapPtr = std::shared_ptr<const Envoy::Http::HeaderMap>;
using RequestGenerator = std::function<HeaderMapPtr()>;

class RequestSource {
public:
  virtual ~RequestSource() = default;
  virtual RequestGenerator get() PURE;
};

using RequestSourcePtr = std::unique_ptr<RequestSource>;

} // namespace Nighthawk
