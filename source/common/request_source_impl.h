#pragma once

#include "envoy/http/header_map.h"

#include "nighthawk/common/request.h"
#include "nighthawk/common/request_source.h"

#include "external/envoy/source/common/common/logger.h"


namespace Nighthawk {

class BaseRequestSourceImpl : public RequestSource,
                              public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
};

class StaticRequestSourceImpl : public BaseRequestSourceImpl {
public:
  StaticRequestSourceImpl(Envoy::Http::HeaderMapPtr&&, const uint64_t max_yields = UINT64_MAX);
  RequestGenerator get() override;

private:
  const HeaderMapPtr header_;
  uint64_t yields_left_;
};

} // namespace Nighthawk
