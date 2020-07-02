#include "server/http_dynamic_delay_filter.h"

#include <string>

#include "envoy/server/filter_config.h"

#include "server/common.h"

#include "absl/strings/str_cat.h"

namespace Nighthawk {
namespace Server {

HttpDynamicDelayDecoderFilterConfig::HttpDynamicDelayDecoderFilterConfig(
    nighthawk::server::ResponseOptions proto_config)
    : server_config_(std::move(proto_config)) {}

HttpDynamicDelayDecoderFilter::HttpDynamicDelayDecoderFilter(
    HttpDynamicDelayDecoderFilterConfigSharedPtr config)
    : config_(std::move(config)) {
  config_->incrementInstanceCount();
}

HttpDynamicDelayDecoderFilter::~HttpDynamicDelayDecoderFilter() {
  RELEASE_ASSERT(destroyed_, "onDestroy() not called");
}

void HttpDynamicDelayDecoderFilter::onDestroy() {
  destroyed_ = true;
  config_->decrementInstanceCount();
}

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
  absl::optional<int64_t> delay_ms;
  if (base_config_.has_static_delay()) {
    delay_ms = Envoy::Protobuf::util::TimeUtil::DurationToMilliseconds(base_config_.static_delay());
  } else if (base_config_.has_concurrency_based_delay()) {
    const nighthawk::server::ConcurrencyBasedDelay& concurrency_config =
        base_config_.concurrency_based_delay();
    const uint64_t current_value = config_->approximateInstances();
    delay_ms = computeDelayMilliseconds(current_value, concurrency_config.minimal_delay(),
                                        concurrency_config.concurrency_delay_factor());
  }
  if (delay_ms.has_value() && delay_ms > 0) {
    // Emit header to communicate the delay we desire to the fault filter extension.
    const Envoy::Http::LowerCaseString key("x-envoy-fault-delay-request");
    headers.setCopy(key, absl::StrCat(*delay_ms));
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
