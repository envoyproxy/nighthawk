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

uint64_t HttpTestServerDecoderFilterConfig::ThreadSafeMontonicTimeStopwatch::getElapsedNsAndReset(
    Envoy::TimeSource& time_source) {
  Envoy::Thread::LockGuard guard(lock_);
  // Note that we obtain monotonic time under lock, to ensure that start_ will be updated
  // monotonically.
  const Envoy::MonotonicTime new_time = time_source.monotonicTime();
  const uint64_t elapsed = start_ == Envoy::MonotonicTime::min() ? 0 : (new_time - start_).count();
  start_ = new_time;
  return elapsed;
}

uint64_t
HttpTestServerDecoderFilterConfig::getElapsedNanosSinceLastRequest(Envoy::TimeSource& time_source) {
  return lastRequestStopwatch().getElapsedNsAndReset(time_source);
}

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
          if (!previous_request_delta_in_response_header.empty() && last_request_delta_ns_ > 0) {
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
  // TODO(oschaaf): this adds locking in the hot path. Consider moving this into a separate
  // extension, which will also allow tracking multiple points via configuration.
  last_request_delta_ns_ =
      config_->getElapsedNanosSinceLastRequest(callbacks.dispatcher().timeSource());
}

} // namespace Server
} // namespace Nighthawk
