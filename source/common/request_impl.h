#pragma once

#include "envoy/http/header_map.h"

#include "nighthawk/common/request.h"

#include "external/envoy/source/common/common/logger.h"

#include "common/request_stream_grpc_client_impl.h"

namespace Nighthawk {

class RequestImpl : public Request {
public:
  RequestImpl(HeaderMapPtr header) : header_(header) {}
  virtual HeaderMapPtr header() { return header_; }

private:
  HeaderMapPtr header_;
};

} // namespace Nighthawk
