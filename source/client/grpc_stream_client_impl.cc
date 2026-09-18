#include "source/client/grpc_stream_client_impl.h"

#include <utility>

#include "external/envoy/source/common/buffer/buffer_impl.h"
#include "external/envoy/source/common/grpc/common.h"
#include "external/envoy/source/common/http/utility.h"

#include "absl/strings/str_cat.h"

namespace Nighthawk {
namespace Client {

using namespace std::chrono_literals;

GrpcStreamBenchmarkClientImpl::GrpcStreamBenchmarkClientImpl(
    Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
    StatisticPtr&& message_latency_statistic, Envoy::Upstream::ClusterManagerPtr& cluster_manager,
    absl::string_view cluster_name, RequestGenerator request_generator, uint32_t streams,
    uint32_t max_inflight_per_stream, std::chrono::nanoseconds drain_duration,
    std::chrono::seconds open_timeout)
    : api_(api), dispatcher_(dispatcher), scope_(scope.createScope("benchmark.")),
      message_latency_statistic_(std::move(message_latency_statistic)),
      cluster_manager_(cluster_manager), cluster_name_(std::string(cluster_name)),
      request_generator_(std::move(request_generator)), stream_count_(streams),
      max_inflight_per_stream_(max_inflight_per_stream), drain_duration_(drain_duration),
      open_timeout_(open_timeout), counters_({ALL_GRPC_STREAM_COUNTERS(POOL_COUNTER(*scope_))}) {
  RELEASE_ASSERT(stream_count_ > 0, "at least one stream is required");
  RELEASE_ASSERT(max_inflight_per_stream_ > 0, "max_inflight_per_stream must be positive");
  message_latency_statistic_->setId("benchmark_stream.message_latency");
  streams_.resize(stream_count_);
  for (uint32_t i = 0; i < stream_count_; i++) {
    streams_[i].handler = std::make_unique<StreamHandler>(*this, i);
  }
}

GrpcStreamBenchmarkClientImpl::~GrpcStreamBenchmarkClientImpl() = default;

std::optional<Envoy::Upstream::HttpPoolData> GrpcStreamBenchmarkClientImpl::pool() {
  const auto thread_local_cluster = cluster_manager_->getThreadLocalCluster(cluster_name_);
  Envoy::Upstream::HostConstSharedPtr host =
      Envoy::Upstream::LoadBalancer::onlyAllowSynchronousHostSelection(
          thread_local_cluster->chooseHost(nullptr));
  return thread_local_cluster->httpConnPool(host, Envoy::Upstream::ResourcePriority::Default,
                                            Envoy::Http::Protocol::Http2, nullptr);
}

void GrpcStreamBenchmarkClientImpl::prepare() {
  RequestPtr request = request_generator_();
  RELEASE_ASSERT(request != nullptr, "the request source did not yield a request");
  request_headers_ = request->header();
  message_ = request->body();

  for (uint32_t i = 0; i < stream_count_; i++) {
    openStream(i);
  }
  if (pending_opens_ == 0) {
    return;
  }
  waiting_for_ = WaitingFor::Opens;
  wait_timer_ = dispatcher_.createTimer([this]() {
    ENVOY_LOG(warn, "Timed out waiting for {} of {} gRPC streams to open.", pending_opens_,
              stream_count_);
    dispatcher_.exit();
  });
  wait_timer_->enableTimer(open_timeout_);
  dispatcher_.run(Envoy::Event::Dispatcher::RunType::RunUntilExit);
  wait_timer_.reset();
  waiting_for_ = WaitingFor::Nothing;
  ENVOY_LOG(info, "Opened {} of {} gRPC bidi streams.", openStreams(), stream_count_);
}

void GrpcStreamBenchmarkClientImpl::openStream(uint32_t index) {
  Stream& stream = streams_[index];
  std::optional<Envoy::Upstream::HttpPoolData> pool_data = pool();
  if (!pool_data.has_value()) {
    stream.state = StreamState::Closed;
    counters_.stream_open_failures_.inc();
    return;
  }
  pending_opens_++;
  // The pool may invoke onPoolReady() synchronously, which is why pending_opens_ is bumped first.
  Envoy::Http::ConnectionPool::Cancellable* cancellable = pool_data.value().newStream(
      *stream.handler, *stream.handler, {/*can_send_early_data_=*/false, /*can_use_http3_=*/false});
  if (stream.state == StreamState::Opening) {
    stream.cancellable = cancellable;
  }
}

void GrpcStreamBenchmarkClientImpl::onStreamReady(uint32_t index,
                                                  Envoy::Http::RequestEncoder& encoder) {
  Stream& stream = streams_[index];
  stream.cancellable = nullptr;
  stream.encoder = &encoder;
  encoder.getStream().addCallbacks(*stream.handler);
  const Envoy::Http::Status status = encoder.encodeHeaders(*request_headers_, /*end_stream=*/false);
  if (!status.ok()) {
    ENVOY_LOG(error, "Failed to encode gRPC stream request headers: {}", status.message());
    stream.state = StreamState::Closed;
    counters_.stream_open_failures_.inc();
  } else {
    stream.state = StreamState::Open;
    counters_.streams_opened_.inc();
  }
  pending_opens_--;
  maybeExitWaitLoop();
}

void GrpcStreamBenchmarkClientImpl::onStreamOpenFailure(
    uint32_t index, Envoy::Http::ConnectionPool::PoolFailureReason reason,
    absl::string_view transport_failure_reason) {
  Stream& stream = streams_[index];
  ENVOY_LOG_EVERY_POW_2(error, "Failed to open gRPC stream {}: reason {} ({})", index,
                        static_cast<int>(reason), transport_failure_reason);
  stream.cancellable = nullptr;
  stream.state = StreamState::Closed;
  counters_.stream_open_failures_.inc();
  pending_opens_--;
  maybeExitWaitLoop();
}

bool GrpcStreamBenchmarkClientImpl::tryStartRequest(CompletionCallback caller_completion_callback) {
  // Open-loop contract: this always "starts" the scheduled message. A message that cannot be
  // sent right now is deferred (dropped, never queued or retried) and completes immediately.
  Stream& stream = streams_[next_stream_];
  next_stream_ = (next_stream_ + 1) % stream_count_;

  if (stream.state != StreamState::Open) {
    counters_.stream_unavailable_.inc();
    dispatcher_.post([cb = std::move(caller_completion_callback)]() { cb(true, false); });
    return true;
  }
  if (stream.write_blocked || stream.inflight.size() >= max_inflight_per_stream_) {
    counters_.stream_deferred_.inc();
    dispatcher_.post([cb = std::move(caller_completion_callback)]() { cb(true, false); });
    return true;
  }

  stream.inflight.push_back(
      {api_.timeSource().monotonicTime(), std::move(caller_completion_callback)});
  Envoy::Buffer::OwnedImpl buffer(message_);
  stream.encoder->encodeData(buffer, /*end_stream=*/false);
  counters_.stream_messages_sent_.inc();
  return true;
}

void GrpcStreamBenchmarkClientImpl::onResponseHeaders(uint32_t index,
                                                      Envoy::Http::ResponseHeaderMapPtr&& headers,
                                                      bool end_stream) {
  Stream& stream = streams_[index];
  const uint64_t response_code = Envoy::Http::Utility::getResponseStatus(*headers);
  if (response_code < 200 || response_code > 299) {
    ENVOY_LOG_EVERY_POW_2(warn, "gRPC stream {} got HTTP status {}", index, response_code);
  }
  if (end_stream) {
    // Trailers-only response: the server closed the stream right away.
    if (stream.state == StreamState::Open) {
      counters_.stream_early_close_.inc();
    }
    closeStream(index, Envoy::Grpc::Common::getGrpcStatus(*headers, /*allow_user_defined=*/true));
  }
}

void GrpcStreamBenchmarkClientImpl::onResponseData(uint32_t index, Envoy::Buffer::Instance& data,
                                                   bool end_stream) {
  Stream& stream = streams_[index];
  std::vector<Envoy::Grpc::Frame> frames;
  const absl::Status status = stream.decoder.decode(data, frames);
  if (!status.ok()) {
    ENVOY_LOG_EVERY_POW_2(error, "gRPC stream {}: failed to decode message frame: {}", index,
                          status.message());
    data.drain(data.length());
  }
  const Envoy::MonotonicTime now = api_.timeSource().monotonicTime();
  for (const Envoy::Grpc::Frame& frame : frames) {
    (void)frame;
    if (stream.inflight.empty()) {
      counters_.stream_unexpected_message_.inc();
      continue;
    }
    InflightMessage message = std::move(stream.inflight.front());
    stream.inflight.pop_front();
    counters_.stream_messages_received_.inc();
    if (measure_latencies_) {
      message_latency_statistic_->addValue((now - message.sent_at).count());
    }
    message.completion_callback(true, true);
  }
  if (end_stream) {
    if (stream.state == StreamState::Open) {
      counters_.stream_early_close_.inc();
    }
    closeStream(index, std::nullopt);
  }
}

void GrpcStreamBenchmarkClientImpl::onResponseTrailers(
    uint32_t index, Envoy::Http::ResponseTrailerMapPtr&& trailers) {
  Stream& stream = streams_[index];
  if (stream.state == StreamState::Open) {
    counters_.stream_early_close_.inc();
  }
  closeStream(index, Envoy::Grpc::Common::getGrpcStatus(*trailers, /*allow_user_defined=*/true));
}

void GrpcStreamBenchmarkClientImpl::onStreamReset(uint32_t index,
                                                  Envoy::Http::StreamResetReason reason,
                                                  absl::string_view transport_failure_reason) {
  Stream& stream = streams_[index];
  if (stream.state == StreamState::Closed) {
    return;
  }
  ENVOY_LOG_EVERY_POW_2(warn, "gRPC stream {} reset: reason {} ({})", index,
                        static_cast<int>(reason), transport_failure_reason);
  counters_.stream_resets_.inc();
  if (stream.state == StreamState::Opening) {
    // A reset while opening counts as an open failure; the pool will not call us again.
    counters_.stream_open_failures_.inc();
    pending_opens_--;
  }
  closeStream(index, std::nullopt);
}

void GrpcStreamBenchmarkClientImpl::onWriteBlocked(uint32_t index, bool blocked) {
  Stream& stream = streams_[index];
  if (blocked && !stream.write_blocked) {
    counters_.stream_write_blocked_.inc();
  }
  stream.write_blocked = blocked;
}

void GrpcStreamBenchmarkClientImpl::closeStream(
    uint32_t index, std::optional<Envoy::Grpc::Status::GrpcStatus> grpc_status) {
  Stream& stream = streams_[index];
  if (stream.state == StreamState::Closed) {
    return;
  }
  stream.state = StreamState::Closed;
  stream.encoder = nullptr;
  grpcStatusCounter(grpc_status).inc();
  completeInflight(stream, /*success=*/false);
  maybeExitWaitLoop();
}

void GrpcStreamBenchmarkClientImpl::completeInflight(Stream& stream, bool success) {
  while (!stream.inflight.empty()) {
    InflightMessage message = std::move(stream.inflight.front());
    stream.inflight.pop_front();
    if (!success) {
      counters_.stream_inflight_lost_.inc();
    }
    message.completion_callback(true, success);
  }
}

Envoy::Stats::Counter& GrpcStreamBenchmarkClientImpl::grpcStatusCounter(
    std::optional<Envoy::Grpc::Status::GrpcStatus> status) {
  auto it = grpc_status_counters_.find(status);
  if (it == grpc_status_counters_.end()) {
    const std::string name = status.has_value()
                                 ? absl::StrCat("stream_grpc_status.", status.value())
                                 : std::string("stream_grpc_status.missing");
    it = grpc_status_counters_.emplace(status, &scope_->counterFromString(name)).first;
  }
  return *it->second;
}

void GrpcStreamBenchmarkClientImpl::maybeExitWaitLoop() {
  switch (waiting_for_) {
  case WaitingFor::Opens:
    if (pending_opens_ == 0) {
      dispatcher_.exit();
    }
    break;
  case WaitingFor::Closes:
    if (openStreams() == 0) {
      dispatcher_.exit();
    }
    break;
  case WaitingFor::Nothing:
    break;
  }
}

uint32_t GrpcStreamBenchmarkClientImpl::openStreams() const {
  uint32_t open = 0;
  for (const Stream& stream : streams_) {
    if (stream.state == StreamState::Open || stream.state == StreamState::HalfClosed) {
      open++;
    }
  }
  return open;
}

void GrpcStreamBenchmarkClientImpl::finish() {
  if (finished_) {
    return;
  }
  finished_ = true;
  // Half-close every open stream: the server sends its remaining echoes and trailers.
  for (Stream& stream : streams_) {
    if (stream.state == StreamState::Open && stream.encoder != nullptr) {
      Envoy::Buffer::OwnedImpl empty;
      stream.encoder->encodeData(empty, /*end_stream=*/true);
      stream.state = StreamState::HalfClosed;
    }
  }
  if (openStreams() == 0) {
    return;
  }
  waiting_for_ = WaitingFor::Closes;
  wait_timer_ = dispatcher_.createTimer([this]() { dispatcher_.exit(); });
  wait_timer_->enableTimer(
      std::chrono::duration_cast<std::chrono::milliseconds>(drain_duration_ + 999ns));
  dispatcher_.run(Envoy::Event::Dispatcher::RunType::RunUntilExit);
  wait_timer_.reset();
  waiting_for_ = WaitingFor::Nothing;
  // Streams the server did not close within the drain window: account for them now, before the
  // worker snapshots its counters. Their unanswered messages are lost, and they will be reset in
  // terminate() without ever yielding a grpc-status.
  uint32_t still_open = 0;
  for (Stream& stream : streams_) {
    if (stream.state == StreamState::Open || stream.state == StreamState::HalfClosed) {
      still_open++;
      counters_.stream_drain_incomplete_.inc();
      completeInflight(stream, /*success=*/false);
    }
  }
  if (still_open > 0) {
    ENVOY_LOG(info,
              "{} gRPC stream(s) still open after the {} ms drain window (counted in "
              "benchmark.stream_drain_incomplete).",
              still_open,
              std::chrono::duration_cast<std::chrono::milliseconds>(drain_duration_).count());
  }
}

void GrpcStreamBenchmarkClientImpl::terminate() {
  finish();
  setShouldMeasureLatencies(false);
  for (uint32_t i = 0; i < stream_count_; i++) {
    Stream& stream = streams_[i];
    if (stream.state == StreamState::Opening && stream.cancellable != nullptr) {
      stream.cancellable->cancel(Envoy::ConnectionPool::CancelPolicy::Default);
      stream.cancellable = nullptr;
      stream.state = StreamState::Closed;
      pending_opens_--;
    } else if (stream.state == StreamState::Open || stream.state == StreamState::HalfClosed) {
      Envoy::Http::RequestEncoder* encoder = stream.encoder;
      stream.state = StreamState::Closed;
      stream.encoder = nullptr;
      completeInflight(stream, /*success=*/false);
      if (encoder != nullptr) {
        encoder->getStream().resetStream(Envoy::Http::StreamResetReason::LocalReset);
      }
    }
  }
}

StatisticPtrMap GrpcStreamBenchmarkClientImpl::statistics() const {
  StatisticPtrMap statistics;
  statistics[message_latency_statistic_->id()] = message_latency_statistic_.get();
  return statistics;
}

} // namespace Client
} // namespace Nighthawk
