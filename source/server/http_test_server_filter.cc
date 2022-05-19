#include "source/server/http_test_server_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "api/server/response_options.pb.validate.h"

#include "source/server/configuration.h"
#include "source/server/well_known_headers.h"

#include "absl/strings/numbers.h"

namespace Nighthawk {
namespace Server {

using ::nighthawk::server::ResponseOptions;
using ::Nighthawk::Server::Configuration::computeEffectiveConfiguration;

HttpTestServerDecoderFilterConfig::HttpTestServerDecoderFilterConfig(
    const ResponseOptions& proto_config)
    : FilterConfigurationBase("test-server"),
      server_config_(std::make_shared<ResponseOptions>(proto_config)) {}

std::shared_ptr<const ResponseOptions> HttpTestServerDecoderFilterConfig::getServerConfig() {
  return server_config_;
}

HttpTestServerDecoderFilter::HttpTestServerDecoderFilter(
    HttpTestServerDecoderFilterConfigSharedPtr config)
    : config_(std::move(config)) {}

void HttpTestServerDecoderFilter::onDestroy() {}

void HttpTestServerDecoderFilter::sendReply(const ResponseOptions& options) {
  std::string response_body(options.response_body_size(), 'a');
  if (request_headers_dump_.has_value()) {
    response_body += *request_headers_dump_;
  }
  decoder_callbacks_->sendLocalReply(
      static_cast<Envoy::Http::Code>(200), response_body,
      [options](Envoy::Http::ResponseHeaderMap& direct_response_headers) {
        Configuration::applyConfigToResponseHeaders(direct_response_headers, options);
      },
      absl::nullopt, "");
}

Envoy::Http::FilterHeadersStatus
HttpTestServerDecoderFilter::decodeHeaders(Envoy::Http::RequestHeaderMap& headers,
                                           bool end_stream) {
  effective_config_ =
      computeEffectiveConfiguration<ResponseOptions>(config_->getServerConfig(), headers);
  if (end_stream) {
    if (!config_->validateOrSendError(effective_config_.status(), *decoder_callbacks_)) {
      if (effective_config_.value()->echo_request_headers()) {
        std::stringstream headers_dump;
        headers_dump << "\nRequest Headers:\n" << headers;
        request_headers_dump_ = headers_dump.str();
      }
      sendReply(*effective_config_.value());
    }
  }
  return Envoy::Http::FilterHeadersStatus::StopIteration;
}

Envoy::Http::FilterDataStatus HttpTestServerDecoderFilter::decodeData(Envoy::Buffer::Instance&,
                                                                      bool end_stream) {
  if (end_stream) {
    if (!config_->validateOrSendError(effective_config_.status(), *decoder_callbacks_)) {
      sendReply(*effective_config_.value());
    }
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

} // namespace Server
} // namespace Nighthawk
