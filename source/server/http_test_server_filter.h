#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "external/envoy/source/common/common/statusor.h"

#include "api/server/response_options.pb.h"

#include "source/server/http_filter_config_base.h"

namespace Nighthawk {
namespace Server {

// Basically this is left in as a placeholder for further configuration.
class HttpTestServerDecoderFilterConfig : public FilterConfigurationBase {
public:
  HttpTestServerDecoderFilterConfig(const nighthawk::server::ResponseOptions& proto_config);

  /**
   * @return std::shared_ptr<const nighthawk::server::TimeTrackingConfiguration> the startup
   * configuration for this filter, which may get overridden by in-flight headers.
   */
  std::shared_ptr<const nighthawk::server::ResponseOptions> getStartupFilterConfiguration();

private:
  std::shared_ptr<const nighthawk::server::ResponseOptions> server_config_;
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
  void sendReply(const nighthawk::server::ResponseOptions& options);
  const HttpTestServerDecoderFilterConfigSharedPtr config_;
  absl::StatusOr<std::shared_ptr<const nighthawk::server::ResponseOptions>> effective_config_;
  Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
  absl::optional<std::string> request_headers_dump_;
};

} // namespace Server
} // namespace Nighthawk
