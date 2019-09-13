#pragma once

#include "envoy/http/header_map.h"

#include "nighthawk/common/header_source.h"

namespace Nighthawk {
namespace Client {

class StaticHeaderSourceImpl : public HeaderSource {
public:
  StaticHeaderSourceImpl(Envoy::Http::HeaderMapPtr&&, const uint64_t max_yields = UINT64_MAX);
  HeaderGenerator get() override;

private:
  const HeaderMapPtr header_;
  uint64_t yields_left_;
};

} // namespace Client
} // namespace Nighthawk
