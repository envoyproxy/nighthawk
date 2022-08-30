#include "source/server/http_dynamic_delay_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "api/server/dynamic_delay.pb.validate.h"

#include "source/server/configuration.h"
#include "source/server/well_known_headers.h"

#include "absl/strings/str_cat.h"

namespace Nighthawk {
namespace Server {
namespace {

using ::nighthawk::server::DynamicDelayConfiguration;
using ::nighthawk::server::ResponseOptions;

// Cherry-picks the relevant fields from the header_json, which should be a ResponseOptions proto,
// and merges them into the base_config.
absl::Status cherryPickDynamicDelayConfiguration(absl::string_view header_json,
                                                 DynamicDelayConfiguration& base_config) {
  try {
    ResponseOptions response_options;
    auto& validation_visitor = Envoy::ProtobufMessage::getStrictValidationVisitor();
    Envoy::MessageUtil::loadFromJson(std::string(header_json), response_options,
                                     validation_visitor);
    switch (response_options.oneof_delay_options_case()) {
    case ResponseOptions::OneofDelayOptionsCase::kStaticDelay:
      *base_config.mutable_static_delay() = response_options.static_delay();
      break;
    case ResponseOptions::OneofDelayOptionsCase::kConcurrencyBasedLinearDelay:
      *base_config.mutable_concurrency_based_linear_delay() =
          response_options.concurrency_based_linear_delay();
      break;
    case ResponseOptions::OneofDelayOptionsCase::ONEOF_DELAY_OPTIONS_NOT_SET:
      break; // No action required.
    }
  } catch (const Envoy::EnvoyException& exception) {
    return absl::InvalidArgumentError(
        fmt::format("Error merging json config: {}", exception.what()));
  }
  return absl::OkStatus();
}

const absl::StatusOr<std::shared_ptr<const DynamicDelayConfiguration>>
computeEffectiveConfiguration(std::shared_ptr<const DynamicDelayConfiguration> base_filter_config,
                              const Envoy::Http::RequestHeaderMap& request_headers) {
  const auto& request_config_header =
      request_headers.get(TestServer::HeaderNames::get().TestServerConfig);
  if (request_config_header.size() == 1) {
    // We could be more flexible and look for the first request header that has a value,
    // but without a proper understanding of a real use case for that, we are assuming that any
    // existence of duplicate headers here is an error.
    DynamicDelayConfiguration modified_filter_config = *base_filter_config;
    absl::Status cherry_pick_status = cherryPickDynamicDelayConfiguration(
        request_config_header[0]->value().getStringView(), modified_filter_config);
    if (cherry_pick_status.ok()) {
      return std::make_shared<const DynamicDelayConfiguration>(std::move(modified_filter_config));
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

HttpDynamicDelayDecoderFilterConfig::HttpDynamicDelayDecoderFilterConfig(
    const DynamicDelayConfiguration& proto_config, Envoy::Runtime::Loader& runtime,
    const std::string& stats_prefix, Envoy::Stats::Scope& scope, Envoy::TimeSource& time_source)
    : FilterConfigurationBase("dynamic-delay"), runtime_(runtime),
      stats_prefix_(absl::StrCat(stats_prefix, fmt::format("{}.", filter_name()))), scope_(scope),
      time_source_(time_source),
      server_config_(std::make_shared<DynamicDelayConfiguration>(proto_config)) {}

std::shared_ptr<const DynamicDelayConfiguration>
HttpDynamicDelayDecoderFilterConfig::getStartupFilterConfiguration() {
  return server_config_;
}

HttpDynamicDelayDecoderFilter::HttpDynamicDelayDecoderFilter(
    HttpDynamicDelayDecoderFilterConfigSharedPtr config)
    : Envoy::Extensions::HttpFilters::Fault::FaultFilter(
          translateOurConfigIntoFaultFilterConfig(*config)),
      config_(std::move(config)) {
  config_->incrementFilterInstanceCount();
}

HttpDynamicDelayDecoderFilter::~HttpDynamicDelayDecoderFilter() {
  RELEASE_ASSERT(destroyed_, "onDestroy() not called");
}

void HttpDynamicDelayDecoderFilter::onDestroy() {
  destroyed_ = true;
  config_->decrementFilterInstanceCount();
  Envoy::Extensions::HttpFilters::Fault::FaultFilter::onDestroy();
}

Envoy::Http::FilterHeadersStatus
HttpDynamicDelayDecoderFilter::decodeHeaders(Envoy::Http::RequestHeaderMap& headers,
                                             bool end_stream) {
  effective_config_ =
      computeEffectiveConfiguration(config_->getStartupFilterConfiguration(), headers);
  if (effective_config_.ok()) {
    const absl::optional<int64_t> delay_ms =
        computeDelayMs(*effective_config_.value(), config_->approximateFilterInstances());
    maybeRequestFaultFilterDelay(delay_ms, headers);
  } else {
    if (end_stream) {
      config_->validateOrSendError(effective_config_.status(), *decoder_callbacks_);
      return Envoy::Http::FilterHeadersStatus::StopIteration;
    }
    return Envoy::Http::FilterHeadersStatus::Continue;
  }
  return Envoy::Extensions::HttpFilters::Fault::FaultFilter::decodeHeaders(headers, end_stream);
}

Envoy::Http::FilterDataStatus
HttpDynamicDelayDecoderFilter::decodeData(Envoy::Buffer::Instance& buffer, bool end_stream) {
  if (!effective_config_.ok()) {
    if (end_stream) {
      config_->validateOrSendError(effective_config_.status(), *decoder_callbacks_);
      return Envoy::Http::FilterDataStatus::StopIterationNoBuffer;
    }
    return Envoy::Http::FilterDataStatus::Continue;
  }
  return Envoy::Extensions::HttpFilters::Fault::FaultFilter::decodeData(buffer, end_stream);
}

absl::optional<int64_t>
HttpDynamicDelayDecoderFilter::computeDelayMs(const DynamicDelayConfiguration& config,
                                              const uint64_t concurrency) {
  absl::optional<int64_t> delay_ms;
  if (config.has_static_delay()) {
    delay_ms = Envoy::Protobuf::util::TimeUtil::DurationToMilliseconds(config.static_delay());
  } else if (config.has_concurrency_based_linear_delay()) {
    const nighthawk::server::ConcurrencyBasedLinearDelay& concurrency_config =
        config.concurrency_based_linear_delay();
    delay_ms = computeConcurrencyBasedLinearDelayMs(concurrency, concurrency_config.minimal_delay(),
                                                    concurrency_config.concurrency_delay_factor());
  }
  return delay_ms;
}

void HttpDynamicDelayDecoderFilter::maybeRequestFaultFilterDelay(
    const absl::optional<int64_t> delay_ms, Envoy::Http::RequestHeaderMap& headers) {
  if (delay_ms.has_value() && delay_ms > 0) {
    // Emit header to communicate the delay we desire to the fault filter extension.
    const Envoy::Http::LowerCaseString key("x-envoy-fault-delay-request");
    headers.setCopy(key, absl::StrCat(*delay_ms));
  }
}

Envoy::Extensions::HttpFilters::Fault::FaultFilterConfigSharedPtr
HttpDynamicDelayDecoderFilter::translateOurConfigIntoFaultFilterConfig(
    HttpDynamicDelayDecoderFilterConfig& config) {
  envoy::extensions::filters::http::fault::v3::HTTPFault fault_config;
  fault_config.mutable_max_active_faults()->set_value(UINT32_MAX);
  fault_config.mutable_delay()->mutable_percentage()->set_numerator(100);
  fault_config.mutable_delay()->mutable_header_delay();
  return std::make_shared<Envoy::Extensions::HttpFilters::Fault::FaultFilterConfig>(
      fault_config, config.runtime(), config.stats_prefix(), config.scope(), config.time_source());
}

void HttpDynamicDelayDecoderFilter::setDecoderFilterCallbacks(
    Envoy::Http::StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
  Envoy::Extensions::HttpFilters::Fault::FaultFilter::setDecoderFilterCallbacks(callbacks);
}

} // namespace Server
} // namespace Nighthawk
