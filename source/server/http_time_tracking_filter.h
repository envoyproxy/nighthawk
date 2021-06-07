#pragma once

#include <string>

#include "envoy/common/time.h"
#include "envoy/server/filter_config.h"

#include "nighthawk/common/stopwatch.h"

#include "external/envoy/source/extensions/filters/http/common/pass_through_filter.h"

#include "api/server/response_options.pb.h"

#include "source/server/http_filter_config_base.h"

namespace Nighthawk {
namespace Server {

/**
 * Filter configuration container class for the time tracking extension.
 * Instances of this class will be shared accross instances of HttpTimeTrackingFilter.
 */
class HttpTimeTrackingFilterConfig : public FilterConfigurationBase {
public:
  /**
   * Constructs a new HttpTimeTrackingFilterConfig instance.
   *
   * @param proto_config The proto configuration of the filter.
   */
  HttpTimeTrackingFilterConfig(const nighthawk::server::ResponseOptions& proto_config);

  /**
   * Gets the number of elapsed nanoseconds since the last call (server wide).
   * Safe to use concurrently.
   *
   * @param time_source Time source that will be used to obain an updated monotonic time sample.
   * @return uint64_t 0 on the first call, else the number of elapsed nanoseconds since the last
   * call.
   */
  uint64_t getElapsedNanosSinceLastRequest(Envoy::TimeSource& time_source);

private:
  std::unique_ptr<Stopwatch> stopwatch_;
};

using HttpTimeTrackingFilterConfigSharedPtr = std::shared_ptr<HttpTimeTrackingFilterConfig>;

/**
 * Extension that tracks elapsed time between inbound requests.
 */
class HttpTimeTrackingFilter : public Envoy::Http::PassThroughFilter {
public:
  /**
   * Construct a new Http Time Tracking Filter object.
   *
   * @param config Configuration of the extension.
   */
  HttpTimeTrackingFilter(HttpTimeTrackingFilterConfigSharedPtr config);

  // Http::StreamDecoderFilter
  Envoy::Http::FilterHeadersStatus decodeHeaders(Envoy::Http::RequestHeaderMap& headers,
                                                 bool /*end_stream*/) override;
  Envoy::Http::FilterDataStatus decodeData(Envoy::Buffer::Instance&, bool) override;
  void setDecoderFilterCallbacks(Envoy::Http::StreamDecoderFilterCallbacks&) override;

  // Http::StreamEncoderFilter
  Envoy::Http::FilterHeadersStatus encodeHeaders(Envoy::Http::ResponseHeaderMap&, bool) override;

private:
  const HttpTimeTrackingFilterConfigSharedPtr config_;
  uint64_t last_request_delta_ns_;
};

} // namespace Server
} // namespace Nighthawk
