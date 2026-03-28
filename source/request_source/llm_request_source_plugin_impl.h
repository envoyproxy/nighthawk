#pragma once

#include <memory>
#include <string>
#include <utility>

#include "source/request_source/llm_request_source_plugin.pb.h"

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

#include "envoy/api/api.h"
#include "envoy/config/core/v3/extension.pb.h"
#include "envoy/http/header_map.h"
#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/options.pb.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/request_source/request_source_plugin_config_factory.h"

namespace Nighthawk {

constexpr inline absl::string_view kLlmRequestSourcePluginName = "nighthawk.request_source.llm";

// A Nighthawk RequestSource that generates completions API requests.
//
// The request source generates requests with the following characteristics:
//   - The request body is a JSON object with the following fields:
//     - model: The name of the model to use for inference.
//     - max_tokens: The maximum number of tokens to return in the response.
//     - messages: A list with a single JSON object containing the following
//       fields:
//       - role: "user"
//       - content: A string containing `req_token_count` randomly generated
//         tokens.
//   - The request headers are copied from the provided header map with the
//     following modifications:
//     - Method: POST
//     - Content-Type: application/json
//     - Content-Length: The length of the request body.
//     - :path: /v1/completions
class LlmRequestSourcePlugin : public Nighthawk::RequestSource,
                               public Envoy::Logger::Loggable<Envoy::Logger::Id::http> {
public:
  explicit LlmRequestSourcePlugin(std::string model_name, int req_token_count, int resp_max_tokens,
                                  Envoy::Http::RequestHeaderMapPtr header)
      : model_name_(model_name), req_token_count_(req_token_count),
        resp_max_tokens_(resp_max_tokens), header_(std::move(header)) {};

  Nighthawk::RequestGenerator get() override;
  void initOnThread() override {};
  void destroyOnThread() override {};

private:
  // Model to use for the request.
  std::string model_name_;
  // Number of tokens to generate in the request.
  int req_token_count_;
  // Maximum number of tokens from the model to return in the response.
  int resp_max_tokens_;
  // The options_list will be used to apply headers to the request.
  std::unique_ptr<const nighthawk::client::RequestOptionsList> options_list_;
  // Headers for the request.
  Envoy::Http::RequestHeaderMapPtr header_;
};

// Factory class for creating LlmRequestSourcePlugin objects.
class LlmRequestSourcePluginFactory : public virtual Nighthawk::RequestSourcePluginConfigFactory {
public:
  std::string name() const override { return std::string(kLlmRequestSourcePluginName); }

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<nighthawk::LlmRequestSourcePluginConfig>();
  }

  Nighthawk::RequestSourcePtr
  createRequestSourcePlugin(const Envoy::Protobuf::Message&, Envoy::Api::Api&,
                            Envoy::Http::RequestHeaderMapPtr header) override;
};

} // namespace Nighthawk
