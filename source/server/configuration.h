#pragma once

#include <string>

#include "envoy/api/v2/core/base.pb.h"
#include "envoy/config/core/v3/base.pb.h"
#include "envoy/http/header_map.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "api/server/response_options.pb.h"

#include "source/server/well_known_headers.h"

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
template <typename ProtoType>
bool mergeJsonConfig(absl::string_view json, ProtoType& config, std::string& error_message) {
  error_message = "";
  try {
    ProtoType json_config;
    auto& validation_visitor = Envoy::ProtobufMessage::getStrictValidationVisitor();
    Envoy::MessageUtil::loadFromJson(std::string(json), json_config, validation_visitor);
    config.MergeFrom(json_config);
    Envoy::MessageUtil::validate(config, validation_visitor);
  } catch (const Envoy::EnvoyException& exception) {
    error_message = fmt::format("Error merging json config: {}", exception.what());
  }
  return error_message == "";
}

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

/**
 * Compute the effective configuration, based on considering the static configuration as well as
 * any configuration provided via request headers.
 *
 * @param base_filter_config Base configuration configured in the server, to be merged with the
 * configuration in the headers
 * @param request_headers Full set of request headers to be inspected for configuration.
 * @return const absl::StatusOr<Envoy::Protobuf::Message> The effective configuration, a proto of
 * the same type as the passed in parameter
 */
template <typename ProtoType>
const absl::StatusOr<std::shared_ptr<const ProtoType>>
computeEffectiveConfiguration(std::shared_ptr<const ProtoType> base_filter_config,
                              const Envoy::Http::RequestHeaderMap& request_headers) {
  const auto& request_config_header =
      request_headers.get(TestServer::HeaderNames::get().TestServerConfig);
  if (request_config_header.size() == 1) {
    // We could be more flexible and look for the first request header that has a value,
    // but without a proper understanding of a real use case for that, we are assuming that any
    // existence of duplicate headers here is an error.
    ProtoType modified_filter_config = *base_filter_config;
    std::string error_message;
    if (mergeJsonConfig(request_config_header[0]->value().getStringView(), modified_filter_config,
                        error_message)) {
      return std::make_shared<const ProtoType>(std::move(modified_filter_config));
    } else {
      return absl::InvalidArgumentError(error_message);
    }
  } else if (request_config_header.size() > 1) {
    return absl::InvalidArgumentError(
        "Received multiple configuration headers in the request, expected only one.");
  }
  return base_filter_config;
}

} // namespace Configuration
} // namespace Server
} // namespace Nighthawk
