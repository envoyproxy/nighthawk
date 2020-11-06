#include "server/http_dynamic_delay_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "server/configuration.h"
#include "server/well_known_headers.h"

#include "absl/strings/str_cat.h"

namespace Nighthawk {
namespace Server {

HttpDynamicDelayDecoderFilterConfig::HttpDynamicDelayDecoderFilterConfig(
    const nighthawk::server::ResponseOptions& proto_config, Envoy::Runtime::Loader& runtime,
    const std::string& stats_prefix, Envoy::Stats::Scope& scope, Envoy::TimeSource& time_source)
    : FilterConfigurationBase(proto_config, "dynamic-delay"), runtime_(runtime),
      stats_prefix_(absl::StrCat(stats_prefix, fmt::format("{}.", filter_name()))), scope_(scope),
      time_source_(time_source) {}

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
  config_->computeEffectiveConfiguration(headers);
  if (config_->getEffectiveConfiguration().ok()) {
    const absl::optional<int64_t> delay_ms = computeDelayMs(
        *config_->getEffectiveConfiguration().value(), config_->approximateFilterInstances());
    maybeRequestFaultFilterDelay(delay_ms, headers);
  } else {
    if (end_stream) {
      config_->validateOrSendError(*decoder_callbacks_);
      return Envoy::Http::FilterHeadersStatus::StopIteration;
    }
    return Envoy::Http::FilterHeadersStatus::Continue;
  }
  return Envoy::Extensions::HttpFilters::Fault::FaultFilter::decodeHeaders(headers, end_stream);
}

Envoy::Http::FilterDataStatus
HttpDynamicDelayDecoderFilter::decodeData(Envoy::Buffer::Instance& buffer, bool end_stream) {
  if (!config_->getEffectiveConfiguration().ok()) {
    if (end_stream) {
      config_->validateOrSendError(*decoder_callbacks_);
      return Envoy::Http::FilterDataStatus::StopIterationNoBuffer;
    }
    return Envoy::Http::FilterDataStatus::Continue;
  }
  return Envoy::Extensions::HttpFilters::Fault::FaultFilter::decodeData(buffer, end_stream);
}

absl::optional<int64_t> HttpDynamicDelayDecoderFilter::computeDelayMs(
    const nighthawk::server::ResponseOptions& response_options, const uint64_t concurrency) {
  absl::optional<int64_t> delay_ms;
  if (response_options.has_static_delay()) {
    delay_ms =
        Envoy::Protobuf::util::TimeUtil::DurationToMilliseconds(response_options.static_delay());
  } else if (response_options.has_concurrency_based_linear_delay()) {
    const nighthawk::server::ConcurrencyBasedLinearDelay& concurrency_config =
        response_options.concurrency_based_linear_delay();
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
