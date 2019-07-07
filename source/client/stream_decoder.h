#pragma once

#include <functional>

#include "envoy/common/time.h"
#include "envoy/event/deferred_deletable.h"
#include "envoy/http/conn_pool.h"

#include "nighthawk/common/statistic.h"

#include "common/pool_impl.h"
#include "common/poolable_impl.h"

namespace Nighthawk {
namespace Client {

class StreamDecoderCompletionCallback {
public:
  virtual ~StreamDecoderCompletionCallback() = default;
  virtual void onComplete(bool success, const Envoy::Http::HeaderMap& headers) PURE;
  virtual void onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason) PURE;
};

class PoolableStreamDecoder;

/**
 * A self destructing response decoder that discards the response body.
 */
class StreamDecoder : public Envoy::Http::StreamDecoder,
                      public Envoy::Http::StreamCallbacks,
                      public Envoy::Http::ConnectionPool::Callbacks,
                      public Envoy::Event::DeferredDeletable {
public:
  StreamDecoder(Envoy::TimeSource& time_source,
                StreamDecoderCompletionCallback& decoder_completion_callback,
                std::function<void()> caller_completion_callback, Statistic& connect_statistic,
                Statistic& latency_statistic, const Envoy::Http::HeaderMap& request_headers,
                bool measure_latencies, uint32_t request_body_size)
      : time_source_(time_source), decoder_completion_callback_(decoder_completion_callback),
        caller_completion_callback_(std::move(caller_completion_callback)),
        connect_statistic_(connect_statistic), latency_statistic_(latency_statistic),
        request_headers_(request_headers), connect_start_(time_source_.monotonicTime()),
        complete_(false), measure_latencies_(measure_latencies),
        request_body_size_(request_body_size) {}

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
                   Envoy::Upstream::HostDescriptionConstSharedPtr host) override;

  // The stream decoder isn't owned by anyone, and self destructs.
  // Our pool implementationn relies on a unique_ptr with a custom deleter.
  // When we must play nice with a pool, we take ownership of that pointer through
  // this call. The pointer will be released upon stream completion, resulting in
  // either a pool recycle or a self destruct.
  void takeOwnership(Nighthawk::PoolImpl<PoolableStreamDecoder>::PoolablePtr self) {
    self_ = std::move(self);
  }

  void setCallerCompletionCallback(std::function<void()> caller_completion_callback) {
    caller_completion_callback_ = caller_completion_callback;
  }

  void reset() { complete_ = false; }
  void setMeasureLatencies(const bool measure_latencies) { measure_latencies_ = measure_latencies; }
  void freeSelf() {
    if (self_.get() == nullptr) {
      // We're not pool-owned, regular self destruct.
      delete this;
    } else {
      // Managed by custome deleter.
      self_ = nullptr;
    }
  }

private:
  void onComplete(bool success);

  Envoy::TimeSource& time_source_;
  StreamDecoderCompletionCallback& decoder_completion_callback_;
  std::function<void()> caller_completion_callback_;
  Statistic& connect_statistic_;
  Statistic& latency_statistic_;
  const Envoy::Http::HeaderMap& request_headers_;
  Envoy::Http::HeaderMapPtr response_headers_;
  const Envoy::MonotonicTime connect_start_;
  Envoy::MonotonicTime request_start_;
  bool complete_;
  bool measure_latencies_{true};
  const uint32_t request_body_size_;
  Nighthawk::PoolImpl<PoolableStreamDecoder>::PoolablePtr self_;
};

class PoolableStreamDecoder : public Client::StreamDecoder, public PoolableImpl {
public:
  PoolableStreamDecoder(Envoy::TimeSource& time_source,
                        Client::StreamDecoderCompletionCallback& decoder_completion_callback,
                        std::function<void()> caller_completion_callback,
                        Statistic& connect_statistic, Statistic& latency_statistic,
                        const Envoy::Http::HeaderMap& request_headers, bool measure_latencies,
                        uint32_t request_body_size)
      : StreamDecoder(time_source, decoder_completion_callback, caller_completion_callback,
                      connect_statistic, latency_statistic, request_headers, measure_latencies,
                      request_body_size) {}
};

class StreamDecoderPoolImpl : public PoolImpl<PoolableStreamDecoder> {
public:
  StreamDecoderPoolImpl(
      StreamDecoderPoolImpl::PoolInstanceConstructionDelegate&& construction_delegate,
      StreamDecoderPoolImpl::PoolInstanceResetDelegate&& reset_delegate)
      : PoolImpl<PoolableStreamDecoder>(std::move(construction_delegate),
                                        std::move(reset_delegate)) {}
};

} // namespace Client
} // namespace Nighthawk
