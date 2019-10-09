#pragma once

#include <functional>

#include "envoy/http/header_map.h"

namespace Nighthawk {

using HeaderMapPtr = std::shared_ptr<const Envoy::Http::HeaderMap>;

class Request {
public:
  virtual ~Request() = default;
  virtual HeaderMapPtr header() PURE;
};

class RequestImpl : public Request {
public:
  RequestImpl(HeaderMapPtr header) : header_(header) {}
  virtual HeaderMapPtr header() { return header_; }

private:
  HeaderMapPtr header_;
};

using RequestPtr = std::unique_ptr<Request>;

using RequestGenerator = std::function<RequestPtr()>;

class RequestSource {
public:
  virtual ~RequestSource() = default;
  virtual RequestGenerator get() PURE;
  virtual void initOnThread() PURE;
};

using RequestSourcePtr = std::unique_ptr<RequestSource>;

} // namespace Nighthawk
