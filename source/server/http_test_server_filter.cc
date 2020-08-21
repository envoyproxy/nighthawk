#include "server/http_test_server_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "server/configuration.h"
#include "server/well_known_headers.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"

namespace Nighthawk {
namespace Server {

HttpTestServerDecoderFilterConfig::HttpTestServerDecoderFilterConfig(
    nighthawk::server::ResponseOptions proto_config)
    : server_config_(std::move(proto_config)) {}

HttpTestServerDecoderFilter::HttpTestServerDecoderFilter(
    HttpTestServerDecoderFilterConfigSharedPtr config)
    : config_(std::move(config)) {}

void HttpTestServerDecoderFilter::onDestroy() {}

void HttpTestServerDecoderFilter::sendReply() {
  if (!json_merge_error_) {
    std::string response_body(base_config_.response_body_size(), 'a');
    if (request_headers_dump_.has_value()) {
      response_body += *request_headers_dump_;
    }
    decoder_callbacks_->sendLocalReply(
        static_cast<Envoy::Http::Code>(200), response_body,
        [this](Envoy::Http::ResponseHeaderMap& direct_response_headers) {
          Configuration::applyConfigToResponseHeaders(direct_response_headers, base_config_);
          const std::string previous_request_delta_in_response_header =
              base_config_.emit_previous_request_delta_in_response_header();
          if (!previous_request_delta_in_response_header.empty()) {
            direct_response_headers.appendCopy(
                Envoy::Http::LowerCaseString(previous_request_delta_in_response_header),
                absl::StrCat(last_request_delta_ns_));
          }
        },
        absl::nullopt, "");
  } else {
    decoder_callbacks_->sendLocalReply(
        static_cast<Envoy::Http::Code>(500),
        fmt::format("test-server didn't understand the request: {}", error_message_), nullptr,
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
    json_merge_error_ = !Configuration::mergeJsonConfig(
        request_config_header->value().getStringView(), base_config_, error_message_);
  }
  if (base_config_.echo_request_headers()) {
    std::stringstream headers_dump;
    headers_dump << "\nRequest Headers:\n" << headers;
    request_headers_dump_ = headers_dump.str();
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
  time_source_ = &callbacks.dispatcher().timeSource();
  const Envoy::MonotonicTime current_time = time_source_->monotonicTime();
  const Envoy::MonotonicTime last_request_time = config_->swapLastRequestTime(current_time);
  last_request_delta_ns_ = (current_time - last_request_time).count();
}

} // namespace Server
} // namespace Nighthawk
