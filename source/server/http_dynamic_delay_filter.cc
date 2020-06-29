#include "server/http_dynamic_delay_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "external/envoy/source/common/stats/symbol_table_impl.h" // For StatName

#include "server/common.h"

#include "absl/strings/str_cat.h"

namespace Nighthawk {
namespace Server {

HttpDynamicDelayDecoderFilterConfig::HttpDynamicDelayDecoderFilterConfig(
    nighthawk::server::ResponseOptions proto_config, Envoy::Stats::Scope& stats_scope)
    : server_config_(std::move(proto_config)), stats_scope_(stats_scope) {}

HttpDynamicDelayDecoderFilter::HttpDynamicDelayDecoderFilter(
    HttpDynamicDelayDecoderFilterConfigSharedPtr config)
    : config_(std::move(config)) {}

void HttpDynamicDelayDecoderFilter::onDestroy() {}

Envoy::Http::FilterHeadersStatus
HttpDynamicDelayDecoderFilter::decodeHeaders(Envoy::Http::RequestHeaderMap& headers, bool) {
  base_config_ = config_->server_config();
  const auto* request_config_header = headers.get(TestServer::HeaderNames::get().TestServerConfig);
  if (request_config_header) {
    Utility::mergeJsonConfig(request_config_header->value().getStringView(), base_config_,
                             error_message_);
  }
  if (error_message_.has_value()) {
    decoder_callbacks_->sendLocalReply(
        static_cast<Envoy::Http::Code>(500),
        fmt::format("dynamic-delay didn't understand the request: {}", *error_message_), nullptr,
        absl::nullopt, "");
    return Envoy::Http::FilterHeadersStatus::StopIteration;
  }
  absl::optional<int64_t> delay;
  if (base_config_.has_static_delay()) {
    delay = Envoy::Protobuf::util::TimeUtil::DurationToMilliseconds(base_config_.static_delay());
  } else if (base_config_.has_gauge_based_delay()) {
    auto& gauge_based_delay = base_config_.gauge_based_delay();
    const std::string gauge_name = gauge_based_delay.gauge_name();
    const uint64_t gauge_value =
        config_->statsScope()
            .gaugeFromString(gauge_name, Envoy::Stats::Gauge::ImportMode::Uninitialized)
            .value();
    const uint64_t gauge_target_value = gauge_based_delay.gauge_target_value();
    const int64_t delay_delta =
        Envoy::Protobuf::util::TimeUtil::DurationToMilliseconds(gauge_based_delay.delay_delta());
    // Compute a delay which linearly decreases with the delta betwen the current and target values
    // of the gauge. We do so by multipling the delta with the configured delta delay.
    // Note that we substract one from the gauge value to avoid inclusion of the the current request
    // in the computation.
    delay = (gauge_target_value - (gauge_value - 1)) * delay_delta;
    if (delay <= 0) {
      delay = absl::nullopt;
    }
  }
  if (delay.has_value()) {
    // Emit header to communicate the delay we desire to the fault filter extension.
    const Envoy::Http::LowerCaseString key("x-envoy-fault-delay-request");
    headers.setCopy(key, absl::StrCat(*delay));
  }
  return Envoy::Http::FilterHeadersStatus::Continue;
}

Envoy::Http::FilterDataStatus HttpDynamicDelayDecoderFilter::decodeData(Envoy::Buffer::Instance&,
                                                                        bool) {
  return Envoy::Http::FilterDataStatus::Continue;
}

Envoy::Http::FilterTrailersStatus
HttpDynamicDelayDecoderFilter::decodeTrailers(Envoy::Http::RequestTrailerMap&) {
  return Envoy::Http::FilterTrailersStatus::Continue;
}

void HttpDynamicDelayDecoderFilter::setDecoderFilterCallbacks(
    Envoy::Http::StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

} // namespace Server
} // namespace Nighthawk
