#pragma once

#include "envoy/http/header_map.h"

#include "nighthawk/common/request_source.h"

namespace Nighthawk {
namespace Client {

class StaticRequestSourceImpl : public RequestSource {
public:
  StaticRequestSourceImpl(Envoy::Http::HeaderMapPtr&&, const uint64_t max_yields = UINT64_MAX);
  RequestGenerator get() override;

private:
  const HeaderMapPtr header_;
  uint64_t yields_left_;
};

} // namespace Client
} // namespace Nighthawk
