#pragma once

#include <string>

#include "external/envoy/envoy/http/header_map.h"
#include "external/envoy/source/common/singleton/const_singleton.h"

namespace Nighthawk {
namespace Server {
namespace TestServer {

class HeaderNameValues {
public:
  const Envoy::Http::LowerCaseString TestServerConfig{"x-nighthawk-test-server-config"};
};

using HeaderNames = Envoy::ConstSingleton<HeaderNameValues>;

} // namespace TestServer
} // namespace Server
} // namespace Nighthawk
