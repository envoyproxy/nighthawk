#pragma once

#include <atomic>
#include <string>

#include "envoy/server/filter_config.h"

#include "api/server/response_options.pb.h"

namespace Nighthawk {
namespace Server {

// Basically this is left in as a placeholder for further configuration.
class HttpDynamicDelayDecoderFilterConfig {
public:
  HttpDynamicDelayDecoderFilterConfig(nighthawk::server::ResponseOptions proto_config);
  const nighthawk::server::ResponseOptions& server_config() { return server_config_; }
  void incrementInstanceCount() { instances()++; }
  void decrementInstanceCount() { instances()--; }
  uint64_t approximateInstances() const { return instances(); }

private:
  const nighthawk::server::ResponseOptions server_config_;
  static std::atomic<uint64_t>& instances() {
    static std::atomic<uint64_t> a(0);
    return a;
  }
};

using HttpDynamicDelayDecoderFilterConfigSharedPtr =
    std::shared_ptr<HttpDynamicDelayDecoderFilterConfig>;

class HttpDynamicDelayDecoderFilter : public Envoy::Http::StreamDecoderFilter {
public:
  HttpDynamicDelayDecoderFilter(HttpDynamicDelayDecoderFilterConfigSharedPtr);

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  Envoy::Http::FilterHeadersStatus decodeHeaders(Envoy::Http::RequestHeaderMap&, bool) override;
  Envoy::Http::FilterDataStatus decodeData(Envoy::Buffer::Instance&, bool) override;
  Envoy::Http::FilterTrailersStatus decodeTrailers(Envoy::Http::RequestTrailerMap&) override;
  void setDecoderFilterCallbacks(Envoy::Http::StreamDecoderFilterCallbacks&) override;

  static int64_t computeDelayMilliseconds(const uint64_t& current_value,
                                          const Envoy::ProtobufWkt::Duration& minimal_delay,
                                          const Envoy::ProtobufWkt::Duration& delay_factor) {
    return std::round(Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(
                          minimal_delay + (current_value * delay_factor)) /
                      1e6);
  }

private:
  const HttpDynamicDelayDecoderFilterConfigSharedPtr config_;
  Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
  nighthawk::server::ResponseOptions base_config_;
  absl::optional<std::string> error_message_;
};

} // namespace Server
} // namespace Nighthawk
