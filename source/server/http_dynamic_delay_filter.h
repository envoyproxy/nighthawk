#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "api/server/response_options.pb.h"

namespace Nighthawk {
namespace Server {

// Basically this is left in as a placeholder for further configuration.
class HttpDynamicDelayDecoderFilterConfig {
public:
  HttpDynamicDelayDecoderFilterConfig(nighthawk::server::ResponseOptions proto_config,
                                      Envoy::Stats::Scope& stats_scope);
  const nighthawk::server::ResponseOptions& server_config() { return server_config_; }

  // Can't const this because of using gaugeFromString()
  Envoy::Stats::Scope& statsScope() { return stats_scope_; }

private:
  const nighthawk::server::ResponseOptions server_config_;
  Envoy::Stats::Scope& stats_scope_;
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

private:
  const HttpDynamicDelayDecoderFilterConfigSharedPtr config_;
  Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
  nighthawk::server::ResponseOptions base_config_;
  absl::optional<std::string> error_message_;
};

} // namespace Server
} // namespace Nighthawk
