#pragma once

#include <string>

#include "envoy/common/time.h"
#include "envoy/server/filter_config.h"

#include "external/envoy/source/common/common/lock_guard.h"
#include "external/envoy/source/common/common/thread.h"

#include "api/server/response_options.pb.h"

namespace Nighthawk {
namespace Server {

/**
 * Filter configuration container class for the test server extension.
 * Instances of this class will be shared accross instances of HttpTestServerDecoderFilter.
 */
class HttpTestServerDecoderFilterConfig {
public:
  /**
   * Constructs a new HttpTestServerDecoderFilterConfig instance.
   *
   * @param proto_config The proto configuration of the filter.
   */
  HttpTestServerDecoderFilterConfig(nighthawk::server::ResponseOptions proto_config);

  /**
   * @return const nighthawk::server::ResponseOptions& read-only reference to the proto config
   * object.
   */
  const nighthawk::server::ResponseOptions& server_config() { return server_config_; }

  /**
   * Gets the number of elapsed nanoseconds since the last call (server wide).
   * Safe to use concurrently.
   *
   * @param time_source Time source that will be used to obain an updated monotonic time sample.
   * @return uint64_t 0 on the first call, else the number of elapsed nanoseconds since the last
   * call.
   */
  static uint64_t getElapsedNanosSinceLastRequest(Envoy::TimeSource& time_source);

private:
  /**
   *  Utility class for thread safe tracking of elapsed monotonic time.
   */
  class ThreadSafeMontonicTimeStopwatch {
  public:
    ThreadSafeMontonicTimeStopwatch() : start_(Envoy::MonotonicTime::min()) {}
    /**
     * @param time_source used to obtain a sample of the current monotonic time.
     * @return uint64_t 0 on the first invocation, and the number of elapsed nanoseconds since the
     * last invocation otherwise.
     */
    uint64_t getElapsedNsAndReset(Envoy::TimeSource& time_source);

  private:
    Envoy::Thread::MutexBasicLockable lock_;
    Envoy::MonotonicTime start_ GUARDED_BY(lock_);
  };

  static ThreadSafeMontonicTimeStopwatch& lastRequestStopwatch() {
    MUTABLE_CONSTRUCT_ON_FIRST_USE(ThreadSafeMontonicTimeStopwatch); // NOLINT
  }
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
  uint64_t last_request_delta_ns_;
};

} // namespace Server
} // namespace Nighthawk
