#include "server/http_dynamic_delay_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "server/common.h"

#include "absl/strings/str_cat.h"

namespace Nighthawk {
namespace Server {

HttpDynamicDelayDecoderFilterConfig::HttpDynamicDelayDecoderFilterConfig(
    nighthawk::server::ResponseOptions proto_config, Envoy::Runtime::Loader& runtime,
    const std::string& stats_prefix, Envoy::Stats::Scope& scope, Envoy::TimeSource& time_source)
    : server_config_(std::move(proto_config)), runtime_(runtime),
      stats_prefix_(absl::StrCat(stats_prefix, "dynamic-delay.")), scope_(scope),
      time_source_(time_source) {}

HttpDynamicDelayDecoderFilter::HttpDynamicDelayDecoderFilter(
    HttpDynamicDelayDecoderFilterConfigSharedPtr config)
    : Envoy::Extensions::HttpFilters::Fault::FaultFilter(
          translateOurConfigIntoFaultFilterConfig(*config)),
      config_(std::move(config)) {
  config_->incrementInstanceCount();
}

HttpDynamicDelayDecoderFilter::~HttpDynamicDelayDecoderFilter() {
  RELEASE_ASSERT(destroyed_, "onDestroy() not called");
  Envoy::Extensions::HttpFilters::Fault::FaultFilter::~FaultFilter();
}

void HttpDynamicDelayDecoderFilter::onDestroy() {
  destroyed_ = true;
  config_->decrementInstanceCount();
  Envoy::Extensions::HttpFilters::Fault::FaultFilter::onDestroy();
}

Envoy::Http::FilterHeadersStatus
HttpDynamicDelayDecoderFilter::decodeHeaders(Envoy::Http::RequestHeaderMap& headers,
                                             bool end_stream) {
  response_options_ = config_->server_config();
  std::string error_message;
  if (!computeResponseOptions(headers, error_message)) {
    decoder_callbacks_->sendLocalReply(
        static_cast<Envoy::Http::Code>(500),
        fmt::format("dynamic-delay didn't understand the request: {}", error_message), nullptr,
        absl::nullopt, "");
    return Envoy::Http::FilterHeadersStatus::StopIteration;
  }
  const absl::optional<int64_t> delay_ms =
      computeDelayMs(response_options_, config_->approximateInstances());
  maybeRequestFaultFilterDelay(delay_ms, headers);
  return Envoy::Extensions::HttpFilters::Fault::FaultFilter::decodeHeaders(headers, end_stream);
}

bool HttpDynamicDelayDecoderFilter::computeResponseOptions(
    const Envoy::Http::RequestHeaderMap& headers, std::string& error_message) {
  response_options_ = config_->server_config();
  const auto* request_config_header = headers.get(TestServer::HeaderNames::get().TestServerConfig);
  if (request_config_header) {
    if (!Utility::mergeJsonConfig(request_config_header->value().getStringView(), response_options_,
                                  error_message)) {
      return false;
    }
  }
  return true;
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
