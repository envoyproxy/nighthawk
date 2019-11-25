#pragma once

#include <functional>

#include "envoy/common/time.h"
#include "envoy/event/deferred_deletable.h"
#include "envoy/event/dispatcher.h"
#include "envoy/http/conn_pool.h"
#include "envoy/server/tracer_config.h"

#include "nighthawk/common/operation_callback.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/common/statistic.h"

#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/runtime/uuid_util.h"
#include "external/envoy/source/common/stream_info/stream_info_impl.h"
#include "external/envoy/source/common/tracing/http_tracer_impl.h"

namespace Nighthawk {
namespace Client {

class StreamDecoderCompletionCallback {
public:
  virtual ~StreamDecoderCompletionCallback() = default;
  virtual void onComplete(bool success, const Envoy::Http::HeaderMap& headers) PURE;
  virtual void onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason) PURE;
};

// TODO(oschaaf): create a StreamDecoderPool?

/**
 * A self destructing response decoder that discards the response body.
 */
class StreamDecoder : public Envoy::Http::StreamDecoder,
                      public Envoy::Http::StreamCallbacks,
                      public Envoy::Http::ConnectionPool::Callbacks,
                      public Envoy::Event::DeferredDeletable {
public:
  StreamDecoder(Envoy::Event::Dispatcher& dispatcher, Envoy::TimeSource& time_source,
                StreamDecoderCompletionCallback& decoder_completion_callback,
                OperationCallback caller_completion_callback, Statistic& connect_statistic,
                Statistic& latency_statistic, HeaderMapPtr request_headers, bool measure_latencies,
                uint32_t request_body_size, std::string x_request_id,
                Envoy::Tracing::HttpTracerPtr& http_tracer)
      : dispatcher_(dispatcher), time_source_(time_source),
        decoder_completion_callback_(decoder_completion_callback),
        caller_completion_callback_(std::move(caller_completion_callback)),
        connect_statistic_(connect_statistic), latency_statistic_(latency_statistic),
        request_headers_(std::move(request_headers)), connect_start_(time_source_.monotonicTime()),
        complete_(false), measure_latencies_(measure_latencies),
        request_body_size_(request_body_size), stream_info_(time_source_),
        http_tracer_(http_tracer) {
    if (measure_latencies_ && http_tracer_ != nullptr) {
      setupForTracing(x_request_id);
    }
  }

  // Http::StreamDecoder
  void decode100ContinueHeaders(Envoy::Http::HeaderMapPtr&&) override {
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  void decodeHeaders(Envoy::Http::HeaderMapPtr&& headers, bool end_stream) override;
  void decodeData(Envoy::Buffer::Instance&, bool end_stream) override;
  void decodeTrailers(Envoy::Http::HeaderMapPtr&& trailers) override;
  void decodeMetadata(Envoy::Http::MetadataMapPtr&&) override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  // Http::StreamCallbacks
  void onResetStream(Envoy::Http::StreamResetReason reason,
                     absl::string_view transport_failure_reason) override;
  void onAboveWriteBufferHighWatermark() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  void onBelowWriteBufferLowWatermark() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  // ConnectionPool::Callbacks
  void onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason,
                     absl::string_view transport_failure_reason,
                     Envoy::Upstream::HostDescriptionConstSharedPtr host) override;
  void onPoolReady(Envoy::Http::StreamEncoder& encoder,
                   Envoy::Upstream::HostDescriptionConstSharedPtr host,
                   const Envoy::StreamInfo::StreamInfo& stream_info) override;

  static Envoy::StreamInfo::ResponseFlag
  streamResetReasonToResponseFlag(Envoy::Http::StreamResetReason reset_reason);
  void finalizeActiveSpan();
  void setupForTracing(std::string& x_request_id);

private:
  void onComplete(bool success);

  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::TimeSource& time_source_;
  StreamDecoderCompletionCallback& decoder_completion_callback_;
  OperationCallback caller_completion_callback_;
  Statistic& connect_statistic_;
  Statistic& latency_statistic_;
  HeaderMapPtr request_headers_;
  Envoy::Http::HeaderMapPtr response_headers_;
  Envoy::Http::HeaderMapPtr trailer_headers_;
  const Envoy::MonotonicTime connect_start_;
  Envoy::MonotonicTime request_start_;
  bool complete_;
  bool measure_latencies_;
  const uint32_t request_body_size_;
  Envoy::Tracing::EgressConfigImpl config_;
  Envoy::StreamInfo::StreamInfoImpl stream_info_;
  Envoy::Tracing::HttpTracerPtr& http_tracer_;
  Envoy::Tracing::SpanPtr active_span_;
  Envoy::StreamInfo::UpstreamTiming upstream_timing_;
};

} // namespace Client
} // namespace Nighthawk
