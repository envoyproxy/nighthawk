#pragma once

#include <string>

#include "envoy/http/header_map.h"

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
