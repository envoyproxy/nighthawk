#include "source/request_source/llm_request_source_plugin_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

#include "source/request_source/llm_request_source_plugin.pb.h"

#include "envoy/api/api.h"
#include "envoy/config/core/v3/base.pb.h"
#include "envoy/config/core/v3/extension.pb.h"
#include "envoy/http/header_map.h"
#include "envoy/registry/registry.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/protobuf/protobuf.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "api/client/options.pb.h"
#include "api/request_source/request_source_plugin.pb.h"
#include "nighthawk/common/request.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/request_source/request_source_plugin_config_factory.h"
#include "source/common/request_impl.h"

namespace Nighthawk {
namespace {

absl::Status ValidateConfig(const nighthawk::LlmRequestSourcePluginConfig& config) {
  if (config.model_name().empty()) {
    return absl::InvalidArgumentError("Model name is required.");
  }

  return absl::OkStatus();
}

std::string GenerateRandomPrompt(int num_tokens) {
  static const char charset[] = "0123456789"
                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "abcdefghijklmnopqrstuvwxyz";

  // The final string to be built.
  std::string result_string;

  // Random number generator for selecting characters from the charset.
  absl::BitGen bitgen;

  for (int i = 0; i < num_tokens; ++i) {
    // Append a random character from the charset.
    result_string += charset[absl::Uniform(bitgen, 0, 62)];

    // Add a space between tokens. This is a naive way to calculate the number
    // of tokens in the string as generally spaces delineate tokens.
    if (i < num_tokens - 1) {
      result_string += ' ';
    }
  }

  return result_string;
}

} // namespace

Nighthawk::RequestGenerator LlmRequestSourcePlugin::get() {
  return [this]() -> std::unique_ptr<Nighthawk::Request> {
    Envoy::Http::RequestHeaderMapPtr headers = Envoy::Http::RequestHeaderMapImpl::create();
    Envoy::Http::HeaderMapImpl::copyFrom(*headers, *header_);

    // TODO(b/436267941): Add support for multiple messages.
    std::string body =
        absl::StrFormat(R"json(
      {
        "model": "%s",
        "max_tokens": %d,
        "messages": [
          {
            "role": "user",
            "content": "%s"
          }
        ]
      }
    )json",
                        model_name_, resp_max_tokens_, GenerateRandomPrompt(req_token_count_));

    headers->setMethod(
        envoy::config::core::v3::RequestMethod_Name(envoy::config::core::v3::RequestMethod::POST));
    headers->setContentType("application/json");
    headers->setContentLength(body.size());

    auto path_key = Envoy::Http::LowerCaseString(":path");
    headers->setCopy(path_key, "/v1/completions");

    ENVOY_LOG(info, body);

    return std::make_unique<Nighthawk::RequestImpl>(std::move(headers), body);
  };
}

Nighthawk::RequestSourcePtr
LlmRequestSourcePluginFactory::createRequestSourcePlugin(const Envoy::Protobuf::Message& message,
                                                         Envoy::Api::Api&,
                                                         Envoy::Http::RequestHeaderMapPtr header) {
  const auto* any = Envoy::Protobuf::DynamicCastToGenerated<const Envoy::Protobuf::Any>(&message);
  nighthawk::LlmRequestSourcePluginConfig llm_config;
  THROW_IF_NOT_OK(Envoy::MessageUtil::unpackTo(*any, llm_config));
  THROW_IF_NOT_OK(ValidateConfig(llm_config));

  for (const nighthawk::client::RequestOptions& request_option :
       llm_config.options_list().options()) {
    for (const envoy::config::core::v3::HeaderValueOption& option_header :
         request_option.request_headers()) {
      auto lower_case_key = Envoy::Http::LowerCaseString(option_header.header().key());
      header->setCopy(lower_case_key, option_header.header().value());
    }
  }

  return std::make_unique<LlmRequestSourcePlugin>(std::string(llm_config.model_name()),
                                                  llm_config.req_token_count(),
                                                  llm_config.resp_max_tokens(), std::move(header));
};

REGISTER_FACTORY(LlmRequestSourcePluginFactory, Nighthawk::RequestSourcePluginConfigFactory);

} // namespace Nighthawk
