#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "external/envoy/source/common/buffer/buffer_impl.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/source/exe/process_wide.h"
#include "external/envoy/test/mocks/http/mocks.h"
#include "external/envoy/test/mocks/stream_info/mocks.h"
#include "external/envoy/test/mocks/upstream/mocks.h"
#include "external/envoy/test/test_common/utility.h"

#include "source/client/grpc_stream_client_impl.h"
#include "source/common/request_impl.h"
#include "source/common/statistic_impl.h"
#include "source/common/utility.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

using namespace std::chrono_literals;

// Frames a serialized message the way a gRPC server echoes it.
std::string echoFrame(absl::string_view message) { return grpcFrameMessage(message); }

ACTION(ReturnNewHostSelectionResponse) { return Envoy::Upstream::HostSelectionResponse(nullptr); }

class GrpcStreamClientTest : public Test {
public:
  GrpcStreamClientTest()
      : api_(Envoy::Api::createApiForTest(time_system_)),
        dispatcher_(api_->allocateDispatcher("test_thread")),
        cluster_manager_(std::make_unique<Envoy::Upstream::MockClusterManager>()),
        cluster_info_(std::make_unique<Envoy::Upstream::MockClusterInfo>()) {
    header_map_ = std::make_shared<Envoy::Http::TestRequestHeaderMapImpl>(
        std::initializer_list<std::pair<std::string, std::string>>{
            {":scheme", "http"},
            {":method", "POST"},
            {":path", "/acme.greeter.Greeter/Chat"},
            {":authority", "localhost"},
            {"content-type", "application/grpc"},
            {"te", "trailers"}});
    EXPECT_CALL(cluster_manager(), getThreadLocalCluster(_))
        .WillRepeatedly(Return(&thread_local_cluster_));
    EXPECT_CALL(thread_local_cluster_, info()).WillRepeatedly(Return(cluster_info_));
    EXPECT_CALL(thread_local_cluster_, chooseHost(_))
        .WillRepeatedly(ReturnNewHostSelectionResponse());
    EXPECT_CALL(thread_local_cluster_, httpConnPool(_, _, _, _))
        .WillRepeatedly(Return(Envoy::Upstream::HttpPoolData([]() {}, &pool_)));
    // Every newStream() hands out a fresh encoder and remembers the decoder/callbacks so the
    // test can play the server side.
    ON_CALL(pool_, newStream(_, _, _))
        .WillByDefault([this](Envoy::Http::ResponseDecoder& decoder,
                              Envoy::Http::ConnectionPool::Callbacks& callbacks,
                              const Envoy::Http::ConnectionPool::Instance::StreamOptions&)
                           -> Envoy::Http::ConnectionPool::Cancellable* {
          decoders_.push_back(&decoder);
          auto encoder = std::make_unique<NiceMock<Envoy::Http::MockRequestEncoder>>();
          ON_CALL(*encoder, encodeHeaders(_, _)).WillByDefault(Return(Envoy::Http::Status()));
          ON_CALL(*encoder, encodeData(_, _))
              .WillByDefault(
                  [this, index = encoders_.size()](Envoy::Buffer::Instance& data, bool end_stream) {
                    sent_data_[index].push_back(data.toString());
                    if (end_stream) {
                      half_closed_[index] = true;
                    }
                  });
          encoders_.push_back(std::move(encoder));
          NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info;
          if (!defer_pool_ready_) {
            callbacks.onPoolReady(*encoders_.back(),
                                  Envoy::Upstream::HostDescriptionConstSharedPtr{}, stream_info,
                                  Envoy::Http::Protocol::Http2);
          } else {
            pending_callbacks_.push_back(&callbacks);
          }
          return nullptr;
        });
  }

  void createClient(uint32_t streams, uint32_t max_inflight = 256,
                    std::chrono::nanoseconds drain = 50ms) {
    RequestGenerator request_generator = [this]() {
      return std::make_unique<RequestImpl>(header_map_, grpcFrameMessage(message_));
    };
    client_ = std::make_unique<GrpcStreamBenchmarkClientImpl>(
        *api_, *dispatcher_, *store_.rootScope(), std::make_unique<StreamingStatistic>(),
        cluster_manager_, "benchmark", request_generator, streams, max_inflight, drain,
        /*open_timeout=*/1s);
    client_->setShouldMeasureLatencies(true);
  }

  uint64_t getCounter(absl::string_view name) {
    return client_->scope().counterFromString(std::string(name)).value();
  }

  Envoy::Upstream::MockClusterManager& cluster_manager() {
    return dynamic_cast<Envoy::Upstream::MockClusterManager&>(*cluster_manager_);
  }

  // Plays the server: echoes count messages on stream index.
  void echo(size_t index, size_t count) {
    Envoy::Buffer::OwnedImpl buffer;
    for (size_t i = 0; i < count; i++) {
      buffer.add(echoFrame(message_));
    }
    decoders_[index]->decodeData(buffer, false);
  }

  void serverHeaders(size_t index) {
    Envoy::Http::ResponseHeaderMapPtr headers{new Envoy::Http::TestResponseHeaderMapImpl{
        {":status", "200"}, {"content-type", "application/grpc"}}};
    decoders_[index]->decodeHeaders(std::move(headers), false);
  }

  void serverTrailers(size_t index, const std::string& grpc_status) {
    Envoy::Http::ResponseTrailerMapPtr trailers{
        new Envoy::Http::TestResponseTrailerMapImpl{{"grpc-status", grpc_status}}};
    decoders_[index]->decodeTrailers(std::move(trailers));
  }

  Envoy::Event::TestRealTimeSystem time_system_;
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Api::ApiPtr api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  Envoy::ProcessWide process_wide_;
  Envoy::Upstream::ClusterManagerPtr cluster_manager_;
  Envoy::Upstream::ClusterInfoConstSharedPtr cluster_info_;
  Envoy::Upstream::MockThreadLocalCluster thread_local_cluster_;
  NiceMock<Envoy::Http::ConnectionPool::MockInstance> pool_;
  std::shared_ptr<Envoy::Http::RequestHeaderMap> header_map_;
  std::string message_{"\x0a\x05world"};
  std::unique_ptr<GrpcStreamBenchmarkClientImpl> client_;
  std::vector<Envoy::Http::ResponseDecoder*> decoders_;
  std::vector<std::unique_ptr<NiceMock<Envoy::Http::MockRequestEncoder>>> encoders_;
  std::vector<Envoy::Http::ConnectionPool::Callbacks*> pending_callbacks_;
  std::map<size_t, std::vector<std::string>> sent_data_;
  std::map<size_t, bool> half_closed_;
  bool defer_pool_ready_{false};
};

TEST_F(GrpcStreamClientTest, PrepareOpensAllStreamsWithHeadersOnly) {
  createClient(3);
  client_->prepare();
  EXPECT_EQ(3, client_->openStreams());
  EXPECT_EQ(3, decoders_.size());
  EXPECT_EQ(3, getCounter("streams_opened"));
  EXPECT_EQ(0, getCounter("stream_open_failures"));
  // No message data was sent while opening.
  EXPECT_TRUE(sent_data_.empty());
}

TEST_F(GrpcStreamClientTest, PrepareWaitsForAsynchronousOpens) {
  defer_pool_ready_ = true;
  createClient(2);
  // Complete the opens from a timer so prepare() has to run the dispatcher.
  Envoy::Event::TimerPtr timer = dispatcher_->createTimer([this]() {
    NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info;
    for (size_t i = 0; i < pending_callbacks_.size(); i++) {
      pending_callbacks_[i]->onPoolReady(*encoders_[i],
                                         Envoy::Upstream::HostDescriptionConstSharedPtr{},
                                         stream_info, Envoy::Http::Protocol::Http2);
    }
  });
  timer->enableTimer(1ms);
  client_->prepare();
  EXPECT_EQ(2, client_->openStreams());
  EXPECT_EQ(2, getCounter("streams_opened"));
}

TEST_F(GrpcStreamClientTest, MessagesRoundRobinOverStreamsAndEchoesCompleteThem) {
  createClient(2);
  client_->prepare();
  int completions = 0;
  int successes = 0;
  CompletionCallback callback = [&](bool complete, bool success) {
    completions += complete ? 1 : 0;
    successes += success ? 1 : 0;
  };
  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(client_->tryStartRequest(callback));
  }
  EXPECT_EQ(4, getCounter("stream_messages_sent"));
  ASSERT_EQ(2, sent_data_[0].size());
  ASSERT_EQ(2, sent_data_[1].size());
  EXPECT_EQ(grpcFrameMessage(message_), sent_data_[0][0]);
  EXPECT_EQ(0, completions);

  serverHeaders(0);
  serverHeaders(1);
  time_system_.advanceTimeWait(2ms);
  echo(0, 2);
  echo(1, 1);
  EXPECT_EQ(3, completions);
  EXPECT_EQ(3, successes);
  EXPECT_EQ(3, getCounter("stream_messages_received"));
  const Statistic& latency = *client_->statistics()["benchmark_stream.message_latency"];
  EXPECT_EQ(3, latency.count());
  EXPECT_GE(latency.min(), std::chrono::nanoseconds(2ms).count());
  EXPECT_EQ(0, getCounter("stream_deferred"));
}

TEST_F(GrpcStreamClientTest, EchoesSplitAcrossDataFramesAreReassembled) {
  createClient(1);
  client_->prepare();
  int completions = 0;
  client_->tryStartRequest([&](bool, bool) { completions++; });
  client_->tryStartRequest([&](bool, bool) { completions++; });
  serverHeaders(0);
  const std::string two_echoes = echoFrame(message_) + echoFrame(message_);
  Envoy::Buffer::OwnedImpl first(two_echoes.substr(0, 3));
  Envoy::Buffer::OwnedImpl second(two_echoes.substr(3));
  decoders_[0]->decodeData(first, false);
  EXPECT_EQ(0, completions);
  decoders_[0]->decodeData(second, false);
  EXPECT_EQ(2, completions);
  EXPECT_EQ(2, getCounter("stream_messages_received"));
}

TEST_F(GrpcStreamClientTest, SendsBeyondInflightBoundAreDeferredNotQueued) {
  createClient(1, /*max_inflight=*/2);
  client_->prepare();
  int deferred_callbacks = 0;
  CompletionCallback callback = [&](bool complete, bool success) {
    if (complete && !success) {
      deferred_callbacks++;
    }
  };
  EXPECT_TRUE(client_->tryStartRequest(callback));
  EXPECT_TRUE(client_->tryStartRequest(callback));
  // Third send hits the bound: open-loop contract says it still "starts", but it is dropped.
  EXPECT_TRUE(client_->tryStartRequest(callback));
  EXPECT_EQ(2, getCounter("stream_messages_sent"));
  EXPECT_EQ(1, getCounter("stream_deferred"));
  EXPECT_EQ(2, sent_data_[0].size());
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::NonBlock);
  EXPECT_EQ(1, deferred_callbacks);
  // One echo frees one slot.
  serverHeaders(0);
  echo(0, 1);
  EXPECT_TRUE(client_->tryStartRequest(callback));
  EXPECT_EQ(3, getCounter("stream_messages_sent"));
  EXPECT_EQ(1, getCounter("stream_deferred"));
}

TEST_F(GrpcStreamClientTest, WriteBufferHighWatermarkDefersSends) {
  createClient(1);
  client_->prepare();
  Envoy::Http::StreamCallbacks* callbacks = nullptr;
  // The client registered itself as the stream's callbacks; fetch it from the mock stream.
  ASSERT_FALSE(encoders_[0]->stream_.callbacks_.empty());
  callbacks = *encoders_[0]->stream_.callbacks_.begin();
  callbacks->onAboveWriteBufferHighWatermark();
  EXPECT_TRUE(client_->tryStartRequest([](bool, bool) {}));
  EXPECT_EQ(1, getCounter("stream_deferred"));
  EXPECT_EQ(1, getCounter("stream_write_blocked"));
  callbacks->onBelowWriteBufferLowWatermark();
  EXPECT_TRUE(client_->tryStartRequest([](bool, bool) {}));
  EXPECT_EQ(1, getCounter("stream_messages_sent"));
}

TEST_F(GrpcStreamClientTest, ServerClosingAStreamEarlyLosesInflightAndRecordsStatus) {
  createClient(2);
  client_->prepare();
  int failed = 0;
  CompletionCallback callback = [&](bool, bool success) { failed += success ? 0 : 1; };
  client_->tryStartRequest(callback); // stream 0
  client_->tryStartRequest(callback); // stream 1
  client_->tryStartRequest(callback); // stream 0
  serverHeaders(0);
  serverTrailers(0, "14");
  EXPECT_EQ(1, client_->openStreams());
  EXPECT_EQ(1, getCounter("stream_early_close"));
  EXPECT_EQ(1, getCounter("stream_grpc_status.14"));
  EXPECT_EQ(2, getCounter("stream_inflight_lost"));
  EXPECT_EQ(2, failed);
  // Later sends scheduled on the dead stream are counted as unavailable, never sent.
  client_->tryStartRequest(callback); // stream 1
  client_->tryStartRequest(callback); // stream 0 (closed)
  EXPECT_EQ(1, getCounter("stream_unavailable"));
  EXPECT_EQ(4, getCounter("stream_messages_sent"));
}

TEST_F(GrpcStreamClientTest, StreamResetIsCounted) {
  createClient(1);
  client_->prepare();
  client_->tryStartRequest([](bool, bool) {});
  (*encoders_[0]->stream_.callbacks_.begin())
      ->onResetStream(Envoy::Http::StreamResetReason::RemoteReset, "");
  EXPECT_EQ(1, getCounter("stream_resets"));
  EXPECT_EQ(1, getCounter("stream_inflight_lost"));
  EXPECT_EQ(1, getCounter("stream_grpc_status.missing"));
  EXPECT_EQ(0, client_->openStreams());
}

TEST_F(GrpcStreamClientTest, FinishHalfClosesAndDrainsUntilStreamsClose) {
  createClient(2, 256, /*drain=*/5s);
  client_->prepare();
  int completions = 0;
  client_->tryStartRequest([&](bool, bool) { completions++; }); // stream 0
  // The server answers during the drain window and then closes both streams.
  Envoy::Event::TimerPtr timer = dispatcher_->createTimer([this]() {
    serverHeaders(0);
    serverHeaders(1);
    echo(0, 1);
    serverTrailers(0, "0");
    serverTrailers(1, "0");
  });
  timer->enableTimer(1ms);
  const Envoy::MonotonicTime started = time_system_.monotonicTime();
  client_->finish();
  const auto took = time_system_.monotonicTime() - started;
  EXPECT_TRUE(half_closed_[0]);
  EXPECT_TRUE(half_closed_[1]);
  EXPECT_EQ(1, completions);
  EXPECT_EQ(1, getCounter("stream_messages_received"));
  EXPECT_EQ(2, getCounter("stream_grpc_status.0"));
  EXPECT_EQ(0, getCounter("stream_early_close"));
  EXPECT_EQ(0, client_->openStreams());
  // Exited on the closes, well before the 5s drain cap.
  EXPECT_LT(took, 4s);
}

TEST_F(GrpcStreamClientTest, FinishGivesUpAfterDrainDuration) {
  createClient(1, 256, /*drain=*/20ms);
  client_->prepare();
  client_->tryStartRequest([](bool, bool) {});
  const Envoy::MonotonicTime started = time_system_.monotonicTime();
  client_->finish();
  EXPECT_GE(time_system_.monotonicTime() - started, 20ms);
  EXPECT_TRUE(half_closed_[0]);
  EXPECT_EQ(1, client_->openStreams());
  // The unanswered message is accounted for by finish() itself, before any counter snapshot.
  EXPECT_EQ(1, getCounter("stream_drain_incomplete"));
  EXPECT_EQ(1, getCounter("stream_inflight_lost"));
  EXPECT_EQ(0, getCounter("stream_grpc_status.missing"));
  // terminate() resets what is left without double counting.
  client_->terminate();
  EXPECT_EQ(0, client_->openStreams());
  EXPECT_EQ(1, getCounter("stream_inflight_lost"));
  EXPECT_EQ(1, getCounter("stream_drain_incomplete"));
}

} // namespace Client
} // namespace Nighthawk
