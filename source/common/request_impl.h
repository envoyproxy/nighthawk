#pragma once

#include "envoy/http/header_map.h"

#include "nighthawk/common/request.h"

namespace Nighthawk {

class RequestImpl : public Request {
public:
  RequestImpl(HeaderMapPtr header) : header_(std::move(header)) {}
  HeaderMapPtr header() const override { return header_; }

private:
  HeaderMapPtr header_;
};

} // namespace Nighthawk
