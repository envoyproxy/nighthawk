#include "server/http_time_tracking_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "server/configuration.h"
#include "server/well_known_headers.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"

namespace Nighthawk {
namespace Server {

HttpTimeTrackingFilterConfig::HttpTimeTrackingFilterConfig(
    nighthawk::server::ResponseOptions proto_config)
    : server_config_(std::move(proto_config)) {}

uint64_t
HttpTimeTrackingFilterConfig::getElapsedNanosSinceLastRequest(Envoy::TimeSource& time_source) {
  return getRequestStopwatch().getElapsedNsAndReset(time_source);
}

HttpTimeTrackingFilter::HttpTimeTrackingFilter(HttpTimeTrackingFilterConfigSharedPtr config)
    : config_(std::move(config)) {}

Envoy::Http::FilterHeadersStatus
HttpTimeTrackingFilter::decodeHeaders(Envoy::Http::RequestHeaderMap& headers, bool /*end_stream*/) {
  base_config_ = config_->server_config();
  const auto* request_config_header = headers.get(TestServer::HeaderNames::get().TestServerConfig);
  if (request_config_header) {
    json_merge_error_ = !Configuration::mergeJsonConfig(
        request_config_header->value().getStringView(), base_config_, error_message_);
    if (json_merge_error_) {
      decoder_callbacks_->sendLocalReply(
          static_cast<Envoy::Http::Code>(500),
          fmt::format("time-tracking didn't understand the request: {}", error_message_), nullptr,
          absl::nullopt, "");
      return Envoy::Http::FilterHeadersStatus::StopIteration;
    }
  }
  return Envoy::Http::FilterHeadersStatus::Continue;
}

Envoy::Http::FilterHeadersStatus
HttpTimeTrackingFilter::encodeHeaders(Envoy::Http::ResponseHeaderMap& response_headers, bool) {
  if (!json_merge_error_) {
    const std::string previous_request_delta_in_response_header =
        base_config_.emit_previous_request_delta_in_response_header();
    if (!previous_request_delta_in_response_header.empty() && last_request_delta_ns_ > 0) {
      response_headers.appendCopy(
          Envoy::Http::LowerCaseString(previous_request_delta_in_response_header),
          absl::StrCat(last_request_delta_ns_));
    }
  }
  return Envoy::Http::FilterHeadersStatus::Continue;
}

void HttpTimeTrackingFilter::setDecoderFilterCallbacks(
    Envoy::Http::StreamDecoderFilterCallbacks& callbacks) {
  Envoy::Http::PassThroughFilter::setDecoderFilterCallbacks(callbacks);
  last_request_delta_ns_ =
      config_->getElapsedNanosSinceLastRequest(callbacks.dispatcher().timeSource());
}

} // namespace Server
} // namespace Nighthawk
