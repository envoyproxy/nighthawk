#pragma once

#include <string>

#include "envoy/http/header_map.h"

#include "external/envoy/source/common/singleton/const_singleton.h"

namespace nighthawk {

class HeaderNameValues {
public:
  const Envoy::Http::LowerCaseString TestServerConfig{"x-nighthawk-test-server-config"};
};

using HeaderNames = Envoy::ConstSingleton<HeaderNameValues>;

} // namespace nighthawk
