#pragma once

#include <string>

#include "envoy/http/header_map.h"

#include "api/server/response_options.pb.h"

namespace Nighthawk {
namespace Server {
namespace Configuration {

/**
 * Merges a json string containing configuration into a ResponseOptions instance.
 *
 * @param json Json-formatted seralization of ResponseOptions to merge into the configuration.
 * @param config The target that the json string should be merged into.
 * @param error_message Set to an error message if one occurred, else set to an empty string.
 * @return bool false if an error occurred.
 */
bool mergeJsonConfig(absl::string_view json, nighthawk::server::ResponseOptions& config,
                     std::string& error_message);

/**
 * Applies ResponseOptions onto a HeaderMap containing response headers.
 *
 * @param response_headers Response headers to transform to reflect the passed in response
 * options.
 * @param response_options Configuration specifying how to transform the header map.
 */
void applyConfigToResponseHeaders(Envoy::Http::ResponseHeaderMap& response_headers,
                                  nighthawk::server::ResponseOptions& response_options);

} // namespace Configuration
} // namespace Server
} // namespace Nighthawk
