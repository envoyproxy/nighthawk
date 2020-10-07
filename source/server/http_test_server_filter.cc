#include "server/http_test_server_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "server/configuration.h"
#include "server/well_known_headers.h"

#include "absl/strings/numbers.h"

namespace Nighthawk {
namespace Server {

HttpTestServerDecoderFilterConfig::HttpTestServerDecoderFilterConfig(
    const nighthawk::server::ResponseOptions& proto_config)
    : FilterConfigurationBase(proto_config, "test-server") {}

HttpTestServerDecoderFilter::HttpTestServerDecoderFilter(
    HttpTestServerDecoderFilterConfigSharedPtr config)
    : config_(std::move(config)) {}

void HttpTestServerDecoderFilter::onDestroy() {}

void HttpTestServerDecoderFilter::sendReply(const nighthawk::server::ResponseOptions& options) {
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
  config_->computeEffectiveConfiguration(headers);
  if (end_stream) {
    if (!config_->maybeSendErrorReply(*decoder_callbacks_)) {
      const absl::StatusOr<EffectiveFilterConfigurationPtr> effective_config =
          config_->getEffectiveConfiguration();
      if (effective_config.value()->echo_request_headers()) {
        std::stringstream headers_dump;
        headers_dump << "\nRequest Headers:\n" << headers;
        request_headers_dump_ = headers_dump.str();
      }
      sendReply(*effective_config.value());
    }
  }
  return Envoy::Http::FilterHeadersStatus::StopIteration;
}

Envoy::Http::FilterDataStatus HttpTestServerDecoderFilter::decodeData(Envoy::Buffer::Instance&,
                                                                      bool end_stream) {
  if (end_stream) {
    if (!config_->maybeSendErrorReply(*decoder_callbacks_)) {
      sendReply(*config_->getEffectiveConfiguration().value());
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

} // namespace Server
} // namespace Nighthawk
