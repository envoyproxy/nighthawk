#include "server/configuration.h"

#include <string>

#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "api/server/response_options.pb.validate.h"

#include "absl/strings/numbers.h"

namespace nighthawk {

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
                                  nighthawk::server::ResponseOptions& response_options) {
  for (const auto& header_value_option : response_options.response_headers()) {
    const auto& header = header_value_option.header();
    auto lower_case_key = Envoy::Http::LowerCaseString(header.key());
    if (!header_value_option.append().value()) {
      response_headers.remove(lower_case_key);
    }
    response_headers.addCopy(lower_case_key, header.value());
  }
}

} // namespace nighthawk
