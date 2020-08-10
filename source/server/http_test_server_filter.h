#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "api/server/response_options.pb.h"

namespace nighthawk {

// Basically this is left in as a placeholder for further configuration.
class HttpTestServerDecoderFilterConfig {
public:
  HttpTestServerDecoderFilterConfig(nighthawk::server::ResponseOptions proto_config);
  const nighthawk::server::ResponseOptions& server_config() { return server_config_; }

private:
  const nighthawk::server::ResponseOptions server_config_;
};

using HttpTestServerDecoderFilterConfigSharedPtr =
    std::shared_ptr<HttpTestServerDecoderFilterConfig>;

class HttpTestServerDecoderFilter : public Envoy::Http::StreamDecoderFilter {
public:
  HttpTestServerDecoderFilter(HttpTestServerDecoderFilterConfigSharedPtr);

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  Envoy::Http::FilterHeadersStatus decodeHeaders(Envoy::Http::RequestHeaderMap&, bool) override;
  Envoy::Http::FilterDataStatus decodeData(Envoy::Buffer::Instance&, bool) override;
  Envoy::Http::FilterTrailersStatus decodeTrailers(Envoy::Http::RequestTrailerMap&) override;
  void setDecoderFilterCallbacks(Envoy::Http::StreamDecoderFilterCallbacks&) override;

private:
  void sendReply();
  const HttpTestServerDecoderFilterConfigSharedPtr config_;
  Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
  nighthawk::server::ResponseOptions base_config_;
  bool json_merge_error_{false};
  std::string error_message_;
  absl::optional<std::string> request_headers_dump_;
};

} // namespace nighthawk
