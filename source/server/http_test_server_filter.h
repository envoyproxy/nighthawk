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

typedef Envoy::ConstSingleton<HeaderNameValues> HeaderNames;

} // namespace TestServer

// Basically this is left in as a placeholder for further configuration.
class HttpTestServerDecoderFilterConfig {
public:
  HttpTestServerDecoderFilterConfig(const nighthawk::server::ResponseOptions& proto_config);
  const nighthawk::server::ResponseOptions& server_config() { return server_config_; }

private:
  const nighthawk::server::ResponseOptions server_config_;
};

typedef std::shared_ptr<HttpTestServerDecoderFilterConfig>
    HttpTestServerDecoderFilterConfigSharedPtr;

class HttpTestServerDecoderFilter : public Envoy::Http::StreamDecoderFilter {
public:
  HttpTestServerDecoderFilter(HttpTestServerDecoderFilterConfigSharedPtr);
  ~HttpTestServerDecoderFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  Envoy::Http::FilterHeadersStatus decodeHeaders(Envoy::Http::HeaderMap&, bool) override;
  Envoy::Http::FilterDataStatus decodeData(Envoy::Buffer::Instance&, bool) override;
  Envoy::Http::FilterTrailersStatus decodeTrailers(Envoy::Http::HeaderMap&) override;
  void setDecoderFilterCallbacks(Envoy::Http::StreamDecoderFilterCallbacks&) override;

private:
  const HttpTestServerDecoderFilterConfigSharedPtr config_;
  Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
};

} // namespace Server
} // namespace Nighthawk
