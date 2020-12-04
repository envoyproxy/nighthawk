#include "server/configuration.h"

#include <string>

#include "envoy/api/v2/core/base.pb.h"
#include "envoy/config/core/v3/base.pb.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "api/server/response_options.pb.validate.h"

#include "absl/strings/numbers.h"

namespace Nighthawk {
namespace Server {
namespace Configuration {

bool mergeJsonConfig(absl::string_view json, nighthawk::server::ResponseOptions& config,
                     std::string& error_message) {
  error_message = "";
  try {
    nighthawk::server::ResponseOptions json_config;
    auto& validation_visitor = Envoy::ProtobufMessage::getStrictValidationVisitor();
    Envoy::MessageUtil::loadFromJson(std::string(json), json_config, validation_visitor);
    config.MergeFrom(json_config);
    Envoy::MessageUtil::validate(config, validation_visitor);
  } catch (const Envoy::EnvoyException& exception) {
    error_message = fmt::format("Error merging json config: {}", exception.what());
  }
  return error_message == "";
}

void applyConfigToResponseHeaders(Envoy::Http::ResponseHeaderMap& response_headers,
                                  const nighthawk::server::ResponseOptions& response_options) {

  // The validation guarantees we only get one of the fields (response_headers, v3_response_headers)
  // set.
  validateResponseOptions(response_options);
  nighthawk::server::ResponseOptions v3_only_response_options = response_options;
  for (const envoy::api::v2::core::HeaderValueOption& header_value_option :
       v3_only_response_options.response_headers()) {
    *v3_only_response_options.add_v3_response_headers() =
        upgradeDeprecatedEnvoyV2HeaderValueOptionToV3(header_value_option);
  }

  for (const envoy::config::core::v3::HeaderValueOption& header_value_option :
       v3_only_response_options.v3_response_headers()) {
    const envoy::config::core::v3::HeaderValue& header = header_value_option.header();
    auto lower_case_key = Envoy::Http::LowerCaseString(header.key());
    if (!header_value_option.append().value()) {
      response_headers.remove(lower_case_key);
    }
    response_headers.addCopy(lower_case_key, header.value());
  }
}

envoy::config::core::v3::HeaderValueOption upgradeDeprecatedEnvoyV2HeaderValueOptionToV3(
    const envoy::api::v2::core::HeaderValueOption& v2_header_value_option) {
  envoy::config::core::v3::HeaderValueOption v3_header_value_option;
  if (v2_header_value_option.has_append()) {
    *v3_header_value_option.mutable_append() = v2_header_value_option.append();
  }
  if (v2_header_value_option.has_header()) {
    envoy::config::core::v3::HeaderValue* v3_header = v3_header_value_option.mutable_header();
    v3_header->set_key(v2_header_value_option.header().key());
    v3_header->set_value(v2_header_value_option.header().value());
  }
  return v3_header_value_option;
}

void validateResponseOptions(const nighthawk::server::ResponseOptions& response_options) {
  if (response_options.response_headers_size() > 0 &&
      response_options.v3_response_headers_size() > 0) {
    throw Envoy::EnvoyException(
        absl::StrCat("invalid configuration in nighthawk::server::ResponseOptions ",
                     "cannot specify both response_headers and v3_response_headers ",
                     "configuration was: ", response_options.ShortDebugString()));
  }
}

} // namespace Configuration
} // namespace Server
} // namespace Nighthawk
