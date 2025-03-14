#include "source/client/stream_decoder.h"

#include <memory>

#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/http/http1/codec_impl.h"
#include "external/envoy/source/common/http/utility.h"
#include "external/envoy/source/common/network/address_impl.h"
#include "external/envoy/source/common/stream_info/stream_info_impl.h"
#include "external/envoy/source/extensions/request_id/uuid/config.h"

#include "fmt/ostream.h"

// NOLINT(namespace-nighthawk)
namespace fmt {
// Allow fmtlib to use operator << defined in HeaderMapPtr.
template <> struct formatter<::Nighthawk::HeaderMapPtr> : ostream_formatter {};
} // namespace fmt

namespace Nighthawk {
namespace Client {

void StreamDecoder::decodeHeaders(Envoy::Http::ResponseHeaderMapPtr&& headers, bool end_stream) {
  ASSERT(!complete_);
  stream_info_.upstreamInfo()->upstreamTiming().onFirstUpstreamRxByteReceived(time_source_);
  complete_ = end_stream;
  response_headers_ = std::move(headers);
  response_header_sizes_statistic_.addValue(response_headers_->byteSize());
  const uint64_t response_code = Envoy::Http::Utility::getResponseStatus(*response_headers_);
  stream_info_.setResponseCode(static_cast<uint32_t>(response_code));
  if (!latency_response_header_name_.empty()) {
    const auto timing_header_name = Envoy::Http::LowerCaseString(latency_response_header_name_);
    const Envoy::Http::HeaderMap::GetResult& timing_header =
        response_headers_->get(timing_header_name);
    if (!timing_header.empty()) {
      absl::string_view timing_value =
          timing_header.size() == 1 ? timing_header[0]->value().getStringView() : "multiple values";
      int64_t origin_delta;
      if (absl::SimpleAtoi(timing_value, &origin_delta) && origin_delta >= 0) {
        origin_latency_statistic_.addValue(origin_delta);
      } else {
        ENVOY_LOG_EVERY_POW_2(warn, "Bad origin delta: '{}'.", timing_value);
      }
    }
  }

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
  decoder_completion_callback_.handleResponseData(data);
}

void StreamDecoder::decodeTrailers(Envoy::Http::ResponseTrailerMapPtr&& headers) {
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
    // At this point StreamDecoder::decodeHeaders() should have been called.
    if (stream_info_.responseCode().has_value()) {
      decoder_completion_callback_.exportLatency(
          stream_info_.responseCode().value(),
          (time_source_.monotonicTime() - request_start_).count());
    } else {
      ENVOY_LOG_EVERY_POW_2(warn, "response_code is not available in onComplete");
    }
  }
  stream_info_.upstreamInfo()->upstreamTiming().onLastUpstreamRxByteReceived(time_source_);
  response_body_sizes_statistic_.addValue(stream_info_.bytesSent());
  stream_info_.onRequestComplete();
  if (response_headers_ != nullptr) {
    decoder_completion_callback_.onComplete(success, *response_headers_);
  } else {
    Envoy::Http::ResponseHeaderMapPtr empty_headers = Envoy::Http::ResponseHeaderMapImpl::create(
        /* max_headers_kb = */ 0, /* max_headers_count = */ 0);
    decoder_completion_callback_.onComplete(success, *empty_headers);
  }
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
  stream_info_.setResponseFlag(Envoy::StreamInfo::CoreResponseFlag::UpstreamConnectionFailure);
  finalizeActiveSpan();
  caller_completion_callback_(false, false);
  dispatcher_.deferredDelete(std::unique_ptr<StreamDecoder>(this));
}

void StreamDecoder::onPoolReady(Envoy::Http::RequestEncoder& encoder,
                                Envoy::Upstream::HostDescriptionConstSharedPtr,
                                Envoy::StreamInfo::StreamInfo&,
                                absl::optional<Envoy::Http::Protocol>) {
  encoder.getStream().addCallbacks(*this);
  stream_info_.upstreamInfo()->upstreamTiming().onFirstUpstreamTxByteSent(
      time_source_); // XXX(oschaaf): is this correct?
  const bool end_stream = request_body_size_ == 0 && request_body_.empty();
  const Envoy::Http::Status status = encoder.encodeHeaders(*request_headers_, end_stream);
  if (!status.ok()) {
    ENVOY_LOG_EVERY_POW_2(error,
                          "Request header encoding failure. Might be missing one or more required "
                          "HTTP headers in {}.",
                          *request_headers_);
  }
  if (request_body_size_ > 0 || !request_body_.empty()) {
    // TODO(https://github.com/envoyproxy/nighthawk/issues/138): This will show up in the zipkin UI
    // as 'response_size'. We add it here, optimistically assuming it will all be send. Ideally,
    // we'd track the encoder events of the stream to dig up and forward more information. For now,
    // we take the risk of erroneously reporting that we did send all the bytes, instead of always
    // reporting 0 bytes.
    Envoy::Buffer::OwnedImpl body_buffer;
    if (request_body_.empty()) {
      // Revisit this when we have non-uniform request distributions and on-the-fly reconfiguration
      // in place. The string size below MUST match the cap we put on
      // RequestOptions::request_body_size in api/client/options.proto!
      stream_info_.addBytesReceived(request_body_size_);
      auto* fragment = new Envoy::Buffer::BufferFragmentImpl(
          staticUploadContent().data(), request_body_size_,
          [](const void*, size_t, const Envoy::Buffer::BufferFragmentImpl* frag) { delete frag; });
      body_buffer.addBufferFragment(*fragment);

    } else {
      stream_info_.addBytesReceived(request_body_.size());
      body_buffer.add(absl::string_view(request_body_));
    }
    encoder.encodeData(body_buffer, true);
  }

  request_start_ = time_source_.monotonicTime();
  if (measure_latencies_) {
    connect_statistic_.addValue((request_start_ - connect_start_).count());
  }
}

// TODO(https://github.com/envoyproxy/nighthawk/issues/139): duplicated from
// envoy/source/common/router/router.cc
Envoy::StreamInfo::CoreResponseFlag
StreamDecoder::streamResetReasonToResponseFlag(Envoy::Http::StreamResetReason reset_reason) {
  switch (reset_reason) {
  case Envoy::Http::StreamResetReason::LocalConnectionFailure:
  case Envoy::Http::StreamResetReason::RemoteConnectionFailure:
  case Envoy::Http::StreamResetReason::ConnectionTimeout:
    return Envoy::StreamInfo::CoreResponseFlag::UpstreamConnectionFailure;
  case Envoy::Http::StreamResetReason::ConnectionTermination:
    return Envoy::StreamInfo::CoreResponseFlag::UpstreamConnectionTermination;
  case Envoy::Http::StreamResetReason::LocalReset:
  case Envoy::Http::StreamResetReason::LocalRefusedStreamReset:
  case Envoy::Http::StreamResetReason::Http1PrematureUpstreamHalfClose:
    return Envoy::StreamInfo::CoreResponseFlag::LocalReset;
  case Envoy::Http::StreamResetReason::Overflow:
    return Envoy::StreamInfo::CoreResponseFlag::UpstreamOverflow;
  case Envoy::Http::StreamResetReason::ConnectError:
  case Envoy::Http::StreamResetReason::RemoteReset:
  case Envoy::Http::StreamResetReason::RemoteRefusedStreamReset:
    return Envoy::StreamInfo::CoreResponseFlag::UpstreamRemoteReset;
  case Envoy::Http::StreamResetReason::ProtocolError:
    return Envoy::StreamInfo::CoreResponseFlag::UpstreamProtocolError;
  case Envoy::Http::StreamResetReason::OverloadManager:
    return Envoy::StreamInfo::CoreResponseFlag::OverloadManager;
  }
  PANIC("not reached");
}

void StreamDecoder::finalizeActiveSpan() {
  if (active_span_ != nullptr) {
    Envoy::Tracing::HttpTracerUtility::finalizeDownstreamSpan(
        *active_span_, request_headers_.get(), response_headers_.get(), trailer_headers_.get(),
        stream_info_, config_);
  }
}

void StreamDecoder::setupForTracing() {
  Envoy::Http::RequestHeaderMapPtr headers_copy = Envoy::Http::RequestHeaderMapImpl::create();
  Envoy::Http::HeaderMapImpl::copyFrom(*headers_copy, *request_headers_);
  Envoy::Tracing::Decision tracing_decision = {Envoy::Tracing::Reason::ClientForced, true};
  envoy::extensions::request_id::uuid::v3::UuidRequestIdConfig uuid_request_id_config;
  Envoy::Extensions::RequestId::UUIDRequestIDExtension uuid_generator(uuid_request_id_config,
                                                                      random_generator_);
  uuid_generator.set(*headers_copy, /* edge_request= */ true, /* keep_external_id= */ false);
  uuid_generator.setTraceReason(*headers_copy, Envoy::Tracing::Reason::ClientForced);
  Envoy::Tracing::HttpTraceContext trace_context(*headers_copy);
  active_span_ = tracer_->startSpan(config_, trace_context, stream_info_, tracing_decision);
  active_span_->injectContext(trace_context, /*upstream=*/nullptr);
  request_headers_.reset(headers_copy.release());
  // We pass in a fake remote address; recently trace finalization mandates setting this, and will
  // segfault without it.
  const auto remote_address = Envoy::Network::Address::InstanceConstSharedPtr{
      new Envoy::Network::Address::Ipv4Instance("127.0.0.1")};
  downstream_address_setter_->setDirectRemoteAddressForTest(remote_address);
}

} // namespace Client
} // namespace Nighthawk
