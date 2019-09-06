#include "client/stream_decoder.h"

#include <memory>

#include "external/envoy/source/common/http/http1/codec_impl.h"
#include "external/envoy/source/common/http/utility.h"

namespace Nighthawk {
namespace Client {

void StreamDecoder::decodeHeaders(Envoy::Http::HeaderMapPtr&& headers, bool end_stream) {
  ASSERT(!complete_);
  complete_ = end_stream;
  response_headers_ = std::move(headers);
  const uint64_t response_code = Envoy::Http::Utility::getResponseStatus(*response_headers_);
  stream_info_.response_code_ = static_cast<uint32_t>(response_code);

  if (complete_) {
    onComplete(true);
  }
}

void StreamDecoder::decodeData(Envoy::Buffer::Instance&, bool end_stream) {
  ASSERT(!complete_);
  complete_ = end_stream;
  if (complete_) {
    onComplete(true);
  }
}

void StreamDecoder::decodeTrailers(Envoy::Http::HeaderMapPtr&&) {
  ASSERT(!complete_);
  complete_ = true;
  onComplete(true);
}

void StreamDecoder::onComplete(bool success) {
  if (success && measure_latencies_) {
    latency_statistic_.addValue((time_source_.monotonicTime() - request_start_).count());
  }
  stream_info_.onRequestComplete();
  ASSERT(!success || complete_);
  decoder_completion_callback_.onComplete(success, *response_headers_);
  if (active_span_.get() != nullptr) {
    // stream_info_.dumpState(std::cerr, 2);
    Envoy::Tracing::HttpTracerUtility::finalizeSpan(*active_span_, &request_headers_, stream_info_,
                                                    config_);
  }
  caller_completion_callback_(complete_, success);
  dispatcher_.deferredDelete(std::unique_ptr<StreamDecoder>(this));
}

void StreamDecoder::onResetStream(Envoy::Http::StreamResetReason,
                                  absl::string_view /* transport_failure_reason */) {
  onComplete(false);
}

void StreamDecoder::onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason,
                                  absl::string_view /* transport_failure_reason */,
                                  Envoy::Upstream::HostDescriptionConstSharedPtr) {
  decoder_completion_callback_.onPoolFailure(reason);
  caller_completion_callback_(false, false);
  dispatcher_.deferredDelete(std::unique_ptr<StreamDecoder>(this));
}

void StreamDecoder::onPoolReady(Envoy::Http::StreamEncoder& encoder,
                                Envoy::Upstream::HostDescriptionConstSharedPtr) {
  encoder.encodeHeaders(request_headers_, request_body_size_ == 0);
  if (request_body_size_ > 0) {
    // TODO(oschaaf): We can probably get away without allocating/copying here if we pregenerate
    // potential request-body candidates up front. Revisit this when we have non-uniform request
    // distributions and on-the-fly reconfiguration in place.
    std::string body(request_body_size_, 'a');
    Envoy::Buffer::OwnedImpl body_buffer(body);
    encoder.encodeData(body_buffer, true);
  }
  request_start_ = time_source_.monotonicTime();
  if (measure_latencies_) {
    connect_statistic_.addValue((request_start_ - connect_start_).count());
  }
}

} // namespace Client
} // namespace Nighthawk
