#pragma once

#include <chrono>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "envoy/api/api.h"
#include "envoy/common/conn_pool.h"
#include "envoy/event/dispatcher.h"
#include "envoy/grpc/status.h"
#include "envoy/http/codec.h"
#include "envoy/http/conn_pool.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"
#include "envoy/upstream/cluster_manager.h"
#include "envoy/upstream/load_balancer.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/common/statistic.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/grpc/codec.h"

#include "absl/container/flat_hash_map.h"

namespace Nighthawk {
namespace Client {

#define ALL_GRPC_STREAM_COUNTERS(COUNTER)                                                          \
  COUNTER(streams_opened)                                                                          \
  COUNTER(stream_open_failures)                                                                    \
  COUNTER(stream_messages_sent)                                                                    \
  COUNTER(stream_messages_received)                                                                \
  COUNTER(stream_deferred)                                                                         \
  COUNTER(stream_unavailable)                                                                      \
  COUNTER(stream_resets)                                                                           \
  COUNTER(stream_early_close)                                                                      \
  COUNTER(stream_unexpected_message)                                                               \
  COUNTER(stream_inflight_lost)                                                                    \
  COUNTER(stream_drain_incomplete)                                                                 \
  COUNTER(stream_write_blocked)

struct GrpcStreamCounters {
  ALL_GRPC_STREAM_COUNTERS(GENERATE_COUNTER_STRUCT)
};

/**
 * BenchmarkClient that drives gRPC bidirectional streaming. It opens a fixed number of long-lived
 * streams in prepare(), then every tryStartRequest() call sends one message on the next stream
 * (round-robin) and completes when the echo for that message arrives on the same stream. Echoes are
 * correlated FIFO per stream, which gRPC's in-order delivery guarantees. A send scheduled for a
 * stream that already has max_inflight_per_stream unanswered messages, or whose write buffer is
 * above the high watermark, is dropped and counted as stream_deferred: it is neither queued nor
 * retried, which keeps the schedule coordinated-omission safe and makes the counter a clean
 * saturation signal. finish() half-closes the streams and collects echoes for the drain duration.
 *
 * Counters live under the "benchmark." scope: stream_messages_sent, stream_messages_received,
 * stream_deferred, stream_unavailable (send scheduled for a stream that is not open),
 * stream_resets, stream_early_close (the server closed a stream before we half-closed it),
 * stream_unexpected_message, stream_inflight_lost (messages unanswered when a stream closed or
 * when the drain window ended), stream_drain_incomplete (streams still open when the drain
 * window ended; their unanswered messages are in stream_inflight_lost and they get no
 * stream_grpc_status entry), stream_write_blocked, streams_opened, stream_open_failures and
 * stream_grpc_status.<code> (grpc-status seen when a stream closed; "missing" when it closed
 * without one).
 * Statistic: benchmark_stream.message_latency.
 */
class GrpcStreamBenchmarkClientImpl : public BenchmarkClient,
                                      public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  /**
   * @param api Envoy api.
   * @param dispatcher the worker's dispatcher.
   * @param scope the worker's stats scope; counters are created under "benchmark." in it.
   * @param message_latency_statistic statistic that records per-message send-to-echo latencies.
   * @param cluster_manager cluster manager holding the worker's cluster.
   * @param cluster_name name of the worker's cluster.
   * @param request_generator yields the request whose headers open the streams and whose body
   * (already gRPC framed) is the message sent on them.
   * @param streams number of bidi streams this worker opens.
   * @param max_inflight_per_stream maximum unanswered messages per stream before sends are
   * deferred.
   * @param drain_duration how long finish() waits for echoes after half-closing.
   * @param open_timeout how long prepare() waits for the streams to open.
   */
  GrpcStreamBenchmarkClientImpl(Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher,
                                Envoy::Stats::Scope& scope,
                                StatisticPtr&& message_latency_statistic,
                                Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                                absl::string_view cluster_name, RequestGenerator request_generator,
                                uint32_t streams, uint32_t max_inflight_per_stream,
                                std::chrono::nanoseconds drain_duration,
                                std::chrono::seconds open_timeout);
  ~GrpcStreamBenchmarkClientImpl() override;

  // BenchmarkClient
  void prepare() override;
  void finish() override;
  void terminate() override;
  void setShouldMeasureLatencies(bool measure_latencies) override {
    measure_latencies_ = measure_latencies;
  }
  bool shouldMeasureLatencies() const override { return measure_latencies_; }
  StatisticPtrMap statistics() const override;
  bool tryStartRequest(CompletionCallback caller_completion_callback) override;
  Envoy::Stats::Scope& scope() const override { return *scope_; }
  std::vector<nighthawk::client::UserDefinedOutput> getUserDefinedOutputResults() const override {
    return {};
  }

  /**
   * @return uint32_t the number of streams currently open (including half-closed ones).
   */
  uint32_t openStreams() const;

private:
  enum class StreamState { Opening, Open, HalfClosed, Closed };

  // Envoy callbacks for a single stream, forwarded to the owning client with the stream index.
  class StreamHandler : public Envoy::Http::ResponseDecoder,
                        public Envoy::Http::StreamCallbacks,
                        public Envoy::Http::ConnectionPool::Callbacks {
  public:
    StreamHandler(GrpcStreamBenchmarkClientImpl& client, uint32_t index)
        : client_(client), index_(index) {}

    // Envoy::Http::ResponseDecoder
    void decode1xxHeaders(Envoy::Http::ResponseHeaderMapPtr&&) override {}
    void decodeHeaders(Envoy::Http::ResponseHeaderMapPtr&& headers, bool end_stream) override {
      client_.onResponseHeaders(index_, std::move(headers), end_stream);
    }
    void decodeData(Envoy::Buffer::Instance& data, bool end_stream) override {
      client_.onResponseData(index_, data, end_stream);
    }
    void decodeTrailers(Envoy::Http::ResponseTrailerMapPtr&& trailers) override {
      client_.onResponseTrailers(index_, std::move(trailers));
    }
    void decodeMetadata(Envoy::Http::MetadataMapPtr&&) override {}
    void dumpState(std::ostream&, int) const override {}
    Envoy::Http::ResponseDecoderHandlePtr createResponseDecoderHandle() override { return nullptr; }

    // Envoy::Http::StreamCallbacks
    void onResetStream(Envoy::Http::StreamResetReason reason,
                       absl::string_view transport_failure_reason) override {
      client_.onStreamReset(index_, reason, transport_failure_reason);
    }
    void onAboveWriteBufferHighWatermark() override { client_.onWriteBlocked(index_, true); }
    void onBelowWriteBufferLowWatermark() override { client_.onWriteBlocked(index_, false); }

    // Envoy::Http::ConnectionPool::Callbacks
    void onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason reason,
                       absl::string_view transport_failure_reason,
                       Envoy::Upstream::HostDescriptionConstSharedPtr) override {
      client_.onStreamOpenFailure(index_, reason, transport_failure_reason);
    }
    void onPoolReady(Envoy::Http::RequestEncoder& encoder,
                     Envoy::Upstream::HostDescriptionConstSharedPtr, Envoy::StreamInfo::StreamInfo&,
                     std::optional<Envoy::Http::Protocol>) override {
      client_.onStreamReady(index_, encoder);
    }

  private:
    GrpcStreamBenchmarkClientImpl& client_;
    const uint32_t index_;
  };

  struct InflightMessage {
    Envoy::MonotonicTime sent_at;
    CompletionCallback completion_callback;
  };

  struct Stream {
    StreamState state{StreamState::Opening};
    Envoy::Http::RequestEncoder* encoder{nullptr};
    Envoy::Http::ConnectionPool::Cancellable* cancellable{nullptr};
    std::unique_ptr<StreamHandler> handler;
    std::deque<InflightMessage> inflight;
    Envoy::Grpc::Decoder decoder;
    bool write_blocked{false};
  };

  std::optional<Envoy::Upstream::HttpPoolData> pool();
  void openStream(uint32_t index);
  void onStreamReady(uint32_t index, Envoy::Http::RequestEncoder& encoder);
  void onStreamOpenFailure(uint32_t index, Envoy::Http::ConnectionPool::PoolFailureReason reason,
                           absl::string_view transport_failure_reason);
  void onResponseHeaders(uint32_t index, Envoy::Http::ResponseHeaderMapPtr&& headers,
                         bool end_stream);
  void onResponseData(uint32_t index, Envoy::Buffer::Instance& data, bool end_stream);
  void onResponseTrailers(uint32_t index, Envoy::Http::ResponseTrailerMapPtr&& trailers);
  void onStreamReset(uint32_t index, Envoy::Http::StreamResetReason reason,
                     absl::string_view transport_failure_reason);
  void onWriteBlocked(uint32_t index, bool blocked);
  // Marks the stream closed, accounts its grpc-status and fails any unanswered messages.
  void closeStream(uint32_t index, std::optional<Envoy::Grpc::Status::GrpcStatus> grpc_status);
  void completeInflight(Stream& stream, bool success);
  Envoy::Stats::Counter& grpcStatusCounter(std::optional<Envoy::Grpc::Status::GrpcStatus> status);
  // Exits the dispatcher run loop started by prepare()/finish() when its condition is met.
  void maybeExitWaitLoop();

  Envoy::Api::Api& api_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::Stats::ScopeSharedPtr scope_;
  StatisticPtr message_latency_statistic_;
  Envoy::Upstream::ClusterManagerPtr& cluster_manager_;
  const std::string cluster_name_;
  const RequestGenerator request_generator_;
  const uint32_t stream_count_;
  const uint32_t max_inflight_per_stream_;
  const std::chrono::nanoseconds drain_duration_;
  const std::chrono::seconds open_timeout_;

  GrpcStreamCounters counters_;
  absl::flat_hash_map<std::optional<Envoy::Grpc::Status::GrpcStatus>, Envoy::Stats::Counter*>
      grpc_status_counters_;
  std::vector<Stream> streams_;
  HeaderMapPtr request_headers_;
  std::string message_;
  uint32_t next_stream_{0};
  uint32_t pending_opens_{0};
  bool measure_latencies_{false};
  // Set while prepare()/finish() run the dispatcher and wait for a condition.
  enum class WaitingFor { Nothing, Opens, Closes };
  WaitingFor waiting_for_{WaitingFor::Nothing};
  Envoy::Event::TimerPtr wait_timer_;
  bool finished_{false};
};

} // namespace Client
} // namespace Nighthawk
