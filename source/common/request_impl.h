#pragma once

#include "nighthawk/common/request.h"

#include "external/envoy/envoy/http/header_map.h"

namespace Nighthawk {

class RequestImpl : public Request {
public:
  RequestImpl(HeaderMapPtr header) : header_(std::move(header)) {}
  HeaderMapPtr header() const override { return header_; }

private:
  HeaderMapPtr header_;
};

} // namespace Nighthawk
