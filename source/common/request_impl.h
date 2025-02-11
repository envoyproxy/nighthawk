#pragma once

#include <string>
#include <utility>
#include "envoy/http/header_map.h"
#include "nighthawk/common/request.h"

namespace Nighthawk {

class RequestImpl : public Request {
public:
  RequestImpl(HeaderMapPtr header, std::string json_body = "")
      : header_(std::move(header)), json_body_(json_body) {}

  HeaderMapPtr header() const override { return header_; }
  const std::string& body() const override { return json_body_; }

private:
  HeaderMapPtr header_;
  std::string json_body_;
};

} // namespace Nighthawk
