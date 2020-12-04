#pragma once

#include <string>

#include "envoy/api/v2/core/base.pb.h"
#include "envoy/config/core/v3/base.pb.h"
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
 *
 * @throws Envoy::EnvoyException if invalid response_options are provided.
 */
void applyConfigToResponseHeaders(Envoy::Http::ResponseHeaderMap& response_headers,
                                  const nighthawk::server::ResponseOptions& response_options);

/**
 * Upgrades Envoy's HeaderValueOption from the deprecated v2 API version to v3.
 *
 * @param v2_header_value_option The HeaderValueOption to be upgraded.
 * @return a version of HeaderValueOption upgraded to Envoy API v3.
 */
envoy::config::core::v3::HeaderValueOption upgradeDeprecatedEnvoyV2HeaderValueOptionToV3(
    const envoy::api::v2::core::HeaderValueOption& v2_header_value_option);

/**
 * Validates the ResponseOptions.
 *
 * @throws Envoy::EnvoyException on validation errors.
 */
void validateResponseOptions(const nighthawk::server::ResponseOptions& response_options);

} // namespace Configuration
} // namespace Server
} // namespace Nighthawk
