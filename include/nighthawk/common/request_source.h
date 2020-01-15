#pragma once

#include <functional>

#include "envoy/http/header_map.h"

#include "nighthawk/common/request.h"

namespace Nighthawk {

using RequestGenerator = std::function<RequestPtr()>;

/**
 * Represents a request source which yields request-specifiers.
 */
class RequestSource {
public:
  virtual ~RequestSource() = default;
  virtual RequestGenerator get() PURE;
  /**
   * Will be called on an intialized and running worker thread, before commencing actual work.
   * Can be used to prepare the request source implementation (opening any connection or files
   * needed, for example).
   */
  virtual void initOnThread() PURE;
};

using RequestSourcePtr = std::unique_ptr<RequestSource>;

} // namespace Nighthawk
