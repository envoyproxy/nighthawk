#pragma once

#include <functional>

#include "envoy/http/header_map.h"

#include "nighthawk/common/request.h"

namespace Nighthawk {

using RequestGenerator = std::function<RequestPtr()>;

class RequestSource {
public:
  virtual ~RequestSource() = default;
  virtual RequestGenerator get() PURE;
  virtual void initOnThread() PURE;
};

using RequestSourcePtr = std::unique_ptr<RequestSource>;

} // namespace Nighthawk
