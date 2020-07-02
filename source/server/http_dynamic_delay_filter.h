#pragma once

#include <atomic>
#include <string>

#include "envoy/server/filter_config.h"

#include "api/server/response_options.pb.h"

namespace Nighthawk {
namespace Server {

/**
 * Filter configuration container class for the dynamic delay extension.
 */
class HttpDynamicDelayDecoderFilterConfig {

public:
  HttpDynamicDelayDecoderFilterConfig(nighthawk::server::ResponseOptions proto_config);

  /**
   * @return const nighthawk::server::ResponseOptions& read-only reference to the proto config
   * object.
   */
  const nighthawk::server::ResponseOptions& server_config() { return server_config_; }

  /**
   * Increments the number of active instances.
   */
  void incrementInstanceCount() { instances()++; }
  /**
   * Decrements the number of active instances.
   */
  void decrementInstanceCount() { instances()--; }
  /**
   * @return uint64_t the approximate number of active instances.
   */
  uint64_t approximateInstances() const { return instances(); }

private:
  const nighthawk::server::ResponseOptions server_config_;
  static std::atomic<uint64_t>& instances() {
    // We lazy-init the atomic to avoid static initialization / a fiasco.
    static std::atomic<uint64_t> a(0);
    return a;
  }
};

using HttpDynamicDelayDecoderFilterConfigSharedPtr =
    std::shared_ptr<HttpDynamicDelayDecoderFilterConfig>;

/**
 * Extension that controls the fault filter extension by supplying it with a request
 * header that triggers it to induce a delay under the hood.
 * In the future, we may look into injecting the fault filter ourselves with the right
 * configuration, either directly/verbatim, or by including a derivation of it in
 * this code base, thereby making it all transparant to the user / eliminating the need
 * to configure the fault filter and make NH take full ownership at the feature level.
 */
class HttpDynamicDelayDecoderFilter : public Envoy::Http::StreamDecoderFilter {
public:
  HttpDynamicDelayDecoderFilter(HttpDynamicDelayDecoderFilterConfigSharedPtr);
  ~HttpDynamicDelayDecoderFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  Envoy::Http::FilterHeadersStatus decodeHeaders(Envoy::Http::RequestHeaderMap&, bool) override;
  Envoy::Http::FilterDataStatus decodeData(Envoy::Buffer::Instance&, bool) override;
  Envoy::Http::FilterTrailersStatus decodeTrailers(Envoy::Http::RequestTrailerMap&) override;
  void setDecoderFilterCallbacks(Envoy::Http::StreamDecoderFilterCallbacks&) override;

  /**
   * Compute the delay in milliseconds based on the parameters provided.
   *
   * @param concurrency indicating the number of active requests.
   * @param minimal_delay gets unconditoinally included in the return value.
   * @param delay_factor added for each increase in the number of active requests.
   * @return int64_t the computed delay in milliseconds.
   */
  static int64_t computeDelayMilliseconds(const uint64_t& concurrency,
                                          const Envoy::ProtobufWkt::Duration& minimal_delay,
                                          const Envoy::ProtobufWkt::Duration& delay_factor) {
    return std::round(Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(
                          minimal_delay + (concurrency * delay_factor)) /
                      1e6);
  }

private:
  const HttpDynamicDelayDecoderFilterConfigSharedPtr config_;
  Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
  nighthawk::server::ResponseOptions base_config_;
  absl::optional<std::string> error_message_;
  bool destroyed_{false};
};

} // namespace Server
} // namespace Nighthawk
