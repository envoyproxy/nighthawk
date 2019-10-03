#include "client/stream_decoder.h"

#include <memory>

#include "external/envoy/source/common/http/http1/codec_impl.h"
#include "external/envoy/source/common/http/utility.h"
#include "external/envoy/source/common/stream_info/stream_info_impl.h"

namespace Nighthawk {
namespace Client {

void StreamDecoder::decodeHeaders(Envoy::Http::HeaderMapPtr&& headers, bool end_stream) {
  ASSERT(!complete_);
  upstream_timing_.onFirstUpstreamRxByteReceived(time_source_);
  complete_ = end_stream;
  response_headers_ = std::move(headers);
  const uint64_t response_code = Envoy::Http::Utility::getResponseStatus(*response_headers_);
  stream_info_.response_code_ = static_cast<uint32_t>(response_code);
  if (complete_) {
    onComplete(true);
  }
}

void StreamDecoder::decodeData(Envoy::Buffer::Instance& data, bool end_stream) {
  ASSERT(!complete_);
  complete_ = end_stream;
  // This will show up in the zipkin UI as 'response_size'. In Envoy this tracks bytes send by Envoy
  // to the downstream.
  stream_info_.addBytesSent(data.length());
  if (complete_) {
    onComplete(true);
  }
}

void StreamDecoder::decodeTrailers(Envoy::Http::HeaderMapPtr&& headers) {
  ASSERT(!complete_);
  complete_ = true;
  if (active_span_ != nullptr) {
    // Save a copy of the trailer headers, as we need them in finalizeActiveSpan()
    trailer_headers_ = std::move(headers);
  }
  onComplete(true);
}

void StreamDecoder::onComplete(bool success) {
  ASSERT(!success || complete_);
  if (success && measure_latencies_) {
    latency_statistic_.addValue((time_source_.monotonicTime() - request_start_).count());
  }
  upstream_timing_.onLastUpstreamRxByteReceived(time_source_);
  stream_info_.onRequestComplete();
  stream_info_.setUpstreamTiming(upstream_timing_);
  decoder_completion_callback_.onComplete(success, *response_headers_);
  finalizeActiveSpan();
  caller_completion_callback_(complete_, success);
  dispatcher_.deferredDelete(std::unique_ptr<StreamDecoder>(this));
}

void StreamDecoder::onResetStream(Envoy::Http::StreamResetReason reason,
                                  absl::string_view /* transport_failure_reason */) {

  stream_info_.setResponseFlag(streamResetReasonToResponseFlag(reason));
  onComplete(false);
}

void StreamDecoder::onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason,
                                  absl::string_view /* transport_failure_reason */,
                                  Envoy::Upstream::HostDescriptionConstSharedPtr) {
  decoder_completion_callback_.onPoolFailure(reason);
  stream_info_.setResponseFlag(Envoy::StreamInfo::ResponseFlag::UpstreamConnectionFailure);
  finalizeActiveSpan();
  caller_completion_callback_(false, false);
  dispatcher_.deferredDelete(std::unique_ptr<StreamDecoder>(this));
}

void StreamDecoder::onPoolReady(Envoy::Http::StreamEncoder& encoder,
                                Envoy::Upstream::HostDescriptionConstSharedPtr,
                                const Envoy::StreamInfo::StreamInfo&) {
  upstream_timing_.onFirstUpstreamTxByteSent(time_source_); // XXX(oschaaf): is this correct?
  encoder.encodeHeaders(*request_headers_, request_body_size_ == 0);
  if (request_body_size_ > 0) {
    // TODO(https://github.com/envoyproxy/nighthawk/issues/138): This will show up in the zipkin UI
    // as 'response_size'. We add it here, optimistically assuming it will all be send. Ideally,
    // we'd track the encoder events of the stream to dig up and forward more information. For now,
    // we take the risk of erroneously reporting that we did send all the bytes, instead of always
    // reporting 0 bytes.
    stream_info_.addBytesReceived(request_body_size_);
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

// TODO(https://github.com/envoyproxy/nighthawk/issues/139): duplicated from the envoy code base.
Envoy::StreamInfo::ResponseFlag
StreamDecoder::streamResetReasonToResponseFlag(Envoy::Http::StreamResetReason reset_reason) {
  switch (reset_reason) {
  case Envoy::Http::StreamResetReason::ConnectionFailure:
    return Envoy::StreamInfo::ResponseFlag::UpstreamConnectionFailure;
  case Envoy::Http::StreamResetReason::ConnectionTermination:
    return Envoy::StreamInfo::ResponseFlag::UpstreamConnectionTermination;
  case Envoy::Http::StreamResetReason::LocalReset:
  case Envoy::Http::StreamResetReason::LocalRefusedStreamReset:
    return Envoy::StreamInfo::ResponseFlag::LocalReset;
  case Envoy::Http::StreamResetReason::Overflow:
    return Envoy::StreamInfo::ResponseFlag::UpstreamOverflow;
  case Envoy::Http::StreamResetReason::RemoteReset:
  case Envoy::Http::StreamResetReason::RemoteRefusedStreamReset:
    return Envoy::StreamInfo::ResponseFlag::UpstreamRemoteReset;
  }
  NOT_REACHED_GCOVR_EXCL_LINE;
}

void StreamDecoder::finalizeActiveSpan() {
  if (active_span_ != nullptr) {
    Envoy::Tracing::HttpTracerUtility::finalizeDownstreamSpan(
        *active_span_, request_headers_.get(), response_headers_.get(), trailer_headers_.get(),
        stream_info_, config_);
  }
}

void StreamDecoder::setupForTracing(std::string& x_request_id) {
  auto headers_copy = std::make_unique<Envoy::Http::HeaderMapImpl>(*request_headers_);
  Envoy::Tracing::Decision tracing_decision = {Envoy::Tracing::Reason::ClientForced, true};
  headers_copy->insertClientTraceId();
  RELEASE_ASSERT(Envoy::UuidUtils::setTraceableUuid(x_request_id, Envoy::UuidTraceStatus::Client),
                 "setTraceableUuid failed");
  headers_copy->ClientTraceId()->value(x_request_id);
  active_span_ = http_tracer_->startSpan(config_, *headers_copy, stream_info_, tracing_decision);
  active_span_->injectContext(*headers_copy);
  request_headers_.reset(headers_copy.release());
}

} // namespace Client
} // namespace Nighthawk
