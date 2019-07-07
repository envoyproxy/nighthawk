#include "client/stream_decoder.h"

#include <memory>

#include "common/http/http1/codec_impl.h"
#include "common/http/utility.h"

namespace Nighthawk {
namespace Client {

void StreamDecoder::decodeHeaders(Envoy::Http::HeaderMapPtr&& headers, bool end_stream) {
  ASSERT(!complete_);
  milestone_tracker_.markMilestone(milestone_response_header_);
  complete_ = end_stream;
  response_headers_ = std::move(headers);
  if (complete_) {
    onComplete(true);
  }
}

void StreamDecoder::decodeData(Envoy::Buffer::Instance&, bool end_stream) {
  ASSERT(!complete_);
  if (!have_first_body_byte_) {
    have_first_body_byte_ = true;
    milestone_tracker_.markMilestone(milestone_first_body_byte_);
  }
  if (end_stream) {
    complete_ = end_stream;
    milestone_tracker_.markMilestone(milestone_last_body_byte_);
    onComplete(true);
  }
}

void StreamDecoder::decodeTrailers(Envoy::Http::HeaderMapPtr&&) {
  ASSERT(!complete_);
  milestone_tracker_.markMilestone(milestone_trailer_);
  complete_ = true;
  onComplete(true);
}

void StreamDecoder::onComplete(bool success) {
  ASSERT(!success || complete_);
  milestone_tracker_.markMilestone(milestone_response_complete_);
  decoder_completion_callback_.onComplete(success, *response_headers_);
  if (success) {
    if (measure_latencies_) {
      latency_statistic_.addValue(
          milestone_tracker_.elapsedBetween(milestone_pool_ready_, milestone_response_complete_)
              .count());
    }
    caller_completion_callback_();
  }

  freeSelf();
}

void StreamDecoder::onResetStream(Envoy::Http::StreamResetReason,
                                  absl::string_view /* transport_failure_reason */) {
  onComplete(false);
}

void StreamDecoder::onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason,
                                  absl::string_view /* transport_failure_reason */,
                                  Envoy::Upstream::HostDescriptionConstSharedPtr) {
  decoder_completion_callback_.onPoolFailure(reason);
  freeSelf();
}

void StreamDecoder::onPoolReady(Envoy::Http::StreamEncoder& encoder,
                                Envoy::Upstream::HostDescriptionConstSharedPtr) {
  milestone_tracker_.markMilestone(milestone_pool_ready_);
  if (measure_latencies_) {
    connect_statistic_.addValue(
        milestone_tracker_.elapsedBetween(milestone_request_start_, milestone_pool_ready_).count());
  }
  encoder.encodeHeaders(request_headers_, request_body_size_ == 0);
  if (request_body_size_ > 0) {
    // TODO(oschaaf): We can probably get away without allocating/copying here if we pregenerate
    // potential request-body candidates up front. Revisit this when we have non-uniform request
    // distributions and on-the-fly reconfiguration in place.
    std::string body(request_body_size_, 'a');
    Envoy::Buffer::OwnedImpl body_buffer(body);
    encoder.encodeData(body_buffer, true);
  }
}

} // namespace Client
} // namespace Nighthawk
