#pragma once

#include <string>

#include "envoy/http/header_map.h"

#include "external/envoy/source/common/singleton/const_singleton.h"

#include "api/server/response_options.pb.h"

namespace Nighthawk {
namespace Server {

namespace TestServer {

class HeaderNameValues {
public:
  const Envoy::Http::LowerCaseString TestServerConfig{"x-nighthawk-test-server-config"};
};

using HeaderNames = Envoy::ConstSingleton<HeaderNameValues>;

} // namespace TestServer

class Utility {
public:
  /**
   * Merges a json string containing configuration into a ResponseOptions instance.
   *
   * @param json Json-formatted seralization of ResponseOptions to merge into the configuration.
   * @param config The target that the json string should be merged into.
   * @param error_message Will contain an error message iff an error occurred.
   * @return bool false iff an error occurred.
   */
  static bool mergeJsonConfig(absl::string_view json, nighthawk::server::ResponseOptions& config,
                              absl::optional<std::string>& error_message);

  /**
   * Applies ResponseOptions onto a HeaderMap containing response headers.
   *
   * @param response_headers Response headers to transform to reflect the passed in response
   * options.
   * @param response_options Configuration specifying how to transform the header map.
   */
  static void applyConfigToResponseHeaders(Envoy::Http::ResponseHeaderMap& response_headers,
                                           nighthawk::server::ResponseOptions& response_options);
};

} // namespace Server
} // namespace Nighthawk
