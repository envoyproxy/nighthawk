#include "source/server/http_time_tracking_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "api/server/response_options.pb.h"
#include "api/server/time_tracking.pb.h"
#include "api/server/time_tracking.pb.validate.h"

#include "source/common/thread_safe_monotonic_time_stopwatch.h"
#include "source/server/well_known_headers.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"

namespace Nighthawk {
namespace Server {

namespace {

using ::nighthawk::server::TimeTrackingConfiguration;

// Cherry-picks the relevant fields from the header_json, which should be a ResponseOptions proto,
// and merges them into the base_config.
absl::Status cherryPickTimeTrackingConfiguration(absl::string_view header_json,
                                                 TimeTrackingConfiguration& base_config) {
  try {
    nighthawk::server::ResponseOptions response_options;
    auto& validation_visitor = Envoy::ProtobufMessage::getStrictValidationVisitor();
    Envoy::MessageUtil::loadFromJson(std::string(header_json), response_options,
                                     validation_visitor);

    if (!response_options.emit_previous_request_delta_in_response_header().empty()) {
      *base_config.mutable_emit_previous_request_delta_in_response_header() =
          response_options.emit_previous_request_delta_in_response_header();
      Envoy::MessageUtil::validate(base_config, validation_visitor);
    }
  } catch (const Envoy::EnvoyException& exception) {
    return absl::InvalidArgumentError(
        fmt::format("Error extracting json config: {}", exception.what()));
  }
  return absl::OkStatus();
}

const absl::StatusOr<std::shared_ptr<const TimeTrackingConfiguration>>
computeEffectiveConfiguration(std::shared_ptr<const TimeTrackingConfiguration> base_filter_config,
                              const Envoy::Http::RequestHeaderMap& request_headers) {
  const auto& request_config_header =
      request_headers.get(TestServer::HeaderNames::get().TestServerConfig);
  if (request_config_header.size() == 1) {
    // We could be more flexible and look for the first request header that has a value,
    // but without a proper understanding of a real use case for that, we are assuming that any
    // existence of duplicate headers here is an error.
    TimeTrackingConfiguration modified_filter_config = *base_filter_config;

    absl::Status cherry_pick_status = cherryPickTimeTrackingConfiguration(
        request_config_header[0]->value().getStringView(), modified_filter_config);

    if (cherry_pick_status.ok()) {
      return std::make_shared<const TimeTrackingConfiguration>(std::move(modified_filter_config));
    } else {
      return cherry_pick_status;
    }
  } else if (request_config_header.size() > 1) {
    return absl::InvalidArgumentError(
        "Received multiple configuration headers in the request, expected only one.");
  }
  return base_filter_config;
}

} // namespace

HttpTimeTrackingFilterConfig::HttpTimeTrackingFilterConfig(
    const TimeTrackingConfiguration& proto_config)
    : FilterConfigurationBase("time-tracking"),
      stopwatch_(std::make_unique<ThreadSafeMontonicTimeStopwatch>()),
      server_config_(std::make_shared<TimeTrackingConfiguration>(proto_config)) {}

uint64_t
HttpTimeTrackingFilterConfig::getElapsedNanosSinceLastRequest(Envoy::TimeSource& time_source) {
  return stopwatch_->getElapsedNsAndReset(time_source);
}

std::shared_ptr<const TimeTrackingConfiguration> HttpTimeTrackingFilterConfig::getServerConfig() {
  return server_config_;
}

HttpTimeTrackingFilter::HttpTimeTrackingFilter(HttpTimeTrackingFilterConfigSharedPtr config)
    : config_(std::move(config)) {}

Envoy::Http::FilterHeadersStatus
HttpTimeTrackingFilter::decodeHeaders(Envoy::Http::RequestHeaderMap& headers, bool end_stream) {
  effective_config_ = computeEffectiveConfiguration(config_->getServerConfig(), headers);
  if (end_stream && config_->validateOrSendError(effective_config_.status(), *decoder_callbacks_)) {
    return Envoy::Http::FilterHeadersStatus::StopIteration;
  }
  return Envoy::Http::FilterHeadersStatus::Continue;
}

Envoy::Http::FilterDataStatus HttpTimeTrackingFilter::decodeData(Envoy::Buffer::Instance&,
                                                                 bool end_stream) {
  if (end_stream && config_->validateOrSendError(effective_config_.status(), *decoder_callbacks_)) {
    return Envoy::Http::FilterDataStatus::StopIterationNoBuffer;
  }
  return Envoy::Http::FilterDataStatus::Continue;
}

Envoy::Http::FilterHeadersStatus
HttpTimeTrackingFilter::encodeHeaders(Envoy::Http::ResponseHeaderMap& response_headers, bool) {
  if (effective_config_.ok()) {
    const std::string previous_request_delta_in_response_header =
        effective_config_.value()->emit_previous_request_delta_in_response_header();
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
