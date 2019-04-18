#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "api/server/response_options.pb.h"

namespace Nighthawk {
namespace Server {

namespace TestServer {

class HeaderNameValues {
public:
  const Envoy::Http::LowerCaseString TestServerConfig{"x-nighthawk-test-server-config"};
};

using HeaderNames = Envoy::ConstSingleton<HeaderNameValues>;

} // namespace TestServer

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
  Envoy::Http::FilterHeadersStatus decodeHeaders(Envoy::Http::HeaderMap&, bool) override;
  Envoy::Http::FilterDataStatus decodeData(Envoy::Buffer::Instance&, bool) override;
  Envoy::Http::FilterTrailersStatus decodeTrailers(Envoy::Http::HeaderMap&) override;
  void setDecoderFilterCallbacks(Envoy::Http::StreamDecoderFilterCallbacks&) override;

  /**
   * Merges a json string containing configuration into a ResponseOptions instance.
   *
   * @param json Json-formatted seralization of ResponseOptions to merge into the configuration.
   * @param config The target that the json string should be merged into.
   * @param error_message Will contain an error message iff an error occurred.
   * @return bool false iff an error occurred.
   */
  bool mergeJsonConfig(absl::string_view json, nighthawk::server::ResponseOptions& config,
                       std::string& error_message);

  /**
   * Applies ResponseOptions onto a HeaderMap containing response headers.
   *
   * @param response_headers Response headers to transform to reflect the passed in response
   * options.
   * @param response_options Configuration specifying how to transform the header map.
   */
  void applyConfigToResponseHeaders(Envoy::Http::HeaderMap& response_headers,
                                    nighthawk::server::ResponseOptions& response_options);

private:
  const HttpTestServerDecoderFilterConfigSharedPtr config_;
  Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
};

} // namespace Server
} // namespace Nighthawk
