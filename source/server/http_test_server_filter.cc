#include "server/http_test_server_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "api/server/response_options.pb.validate.h"

#include "absl/strings/numbers.h"

namespace Nighthawk {
namespace Server {

HttpTestServerDecoderFilterConfig::HttpTestServerDecoderFilterConfig(
    nighthawk::server::ResponseOptions proto_config)
    : server_config_(std::move(proto_config)) {}

HttpTestServerDecoderFilter::HttpTestServerDecoderFilter(
    HttpTestServerDecoderFilterConfigSharedPtr config)
    : config_(std::move(config)) {}

void HttpTestServerDecoderFilter::onDestroy() {}

bool HttpTestServerDecoderFilter::mergeJsonConfig(absl::string_view json,
                                                  nighthawk::server::ResponseOptions& config,
                                                  absl::optional<std::string>& error_message) {
  error_message = absl::nullopt;
  try {
    nighthawk::server::ResponseOptions json_config;
    auto& validation_visitor = Envoy::ProtobufMessage::getStrictValidationVisitor();
    Envoy::MessageUtil::loadFromJson(std::string(json), json_config, validation_visitor);
    config.MergeFrom(json_config);
    Envoy::MessageUtil::validate(config, validation_visitor);
  } catch (const Envoy::EnvoyException& exception) {
    error_message.emplace(fmt::format("Error merging json config: {}", exception.what()));
  }
  return error_message == absl::nullopt;
}

void HttpTestServerDecoderFilter::applyConfigToResponseHeaders(
    Envoy::Http::ResponseHeaderMap& response_headers,
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

void HttpTestServerDecoderFilter::sendReply() {
  if (error_message_ == absl::nullopt) {
    decoder_callbacks_->sendLocalReply(
        static_cast<Envoy::Http::Code>(200), std::string(base_config_.response_body_size(), 'a'),
        [this](Envoy::Http::ResponseHeaderMap& direct_response_headers) {
          applyConfigToResponseHeaders(direct_response_headers, base_config_);
        },
        absl::nullopt, "");
  } else {
    decoder_callbacks_->sendLocalReply(
        static_cast<Envoy::Http::Code>(500),
        fmt::format("test-server didn't understand the request: {}", *error_message_), nullptr,
        absl::nullopt, "");
  }
}

Envoy::Http::FilterHeadersStatus
HttpTestServerDecoderFilter::decodeHeaders(Envoy::Http::RequestHeaderMap& headers,
                                           bool end_stream) {
  // TODO(oschaaf): Add functionality to clear fields
  base_config_ = config_->server_config();
  const auto* request_config_header = headers.get(TestServer::HeaderNames::get().TestServerConfig);
  if (request_config_header) {
    mergeJsonConfig(request_config_header->value().getStringView(), base_config_, error_message_);
  }
  if (end_stream) {
    sendReply();
  }
  return Envoy::Http::FilterHeadersStatus::StopIteration;
}

Envoy::Http::FilterDataStatus HttpTestServerDecoderFilter::decodeData(Envoy::Buffer::Instance&,
                                                                      bool end_stream) {
  if (end_stream) {
    sendReply();
  }
  return Envoy::Http::FilterDataStatus::StopIterationNoBuffer;
}

Envoy::Http::FilterTrailersStatus
HttpTestServerDecoderFilter::decodeTrailers(Envoy::Http::RequestTrailerMap&) {
  return Envoy::Http::FilterTrailersStatus::Continue;
}

void HttpTestServerDecoderFilter::setDecoderFilterCallbacks(
    Envoy::Http::StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

} // namespace Server
} // namespace Nighthawk
