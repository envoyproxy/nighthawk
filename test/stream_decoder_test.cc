#include <chrono>

#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/test/mocks/http/mocks.h"

#include "common/statistic_impl.h"

#include "client/stream_decoder.h"

#include "test/mocks.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

class StreamDecoderTest : public Test, public StreamDecoderCompletionCallback {
public:
  StreamDecoderTest()
      : api_(Envoy::Api::createApiForTest()), dispatcher_(api_->allocateDispatcher()) {}

  void onComplete(bool, const Envoy::Http::HeaderMap&) override {
    stream_decoder_completion_callbacks_++;
  }
  void onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason) override { pool_failures_++; }

  Envoy::Event::TestRealTimeSystem time_system_;
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Api::ApiPtr api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  StreamingStatistic connect_statistic_;
  StreamingStatistic latency_statistic_;
  Envoy::Http::HeaderMapImpl request_headers_;
  uint64_t stream_decoder_completion_callbacks_{0};
  uint64_t pool_failures_{0};
  // XXX(oschaaf): mock tracer, set expectations
  Envoy::Tracing::HttpNullTracer http_tracer_;
};

TEST_F(StreamDecoderTest, HeaderOnlyTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete]() { is_complete = true; },
      connect_statistic_, latency_statistic_, request_headers_, false, 0, "654", http_tracer_);
  Envoy::Http::HeaderMapPtr headers{new Envoy::Http::TestHeaderMapImpl{{":status", "200"}}};
  decoder->decodeHeaders(std::move(headers), true);
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, HeaderWithBodyTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete]() { is_complete = true; },
      connect_statistic_, latency_statistic_, request_headers_, false, 0, "654", http_tracer_);
  Envoy::Http::HeaderMapPtr headers{new Envoy::Http::TestHeaderMapImpl{{":status", "200"}}};
  decoder->decodeHeaders(std::move(headers), false);
  EXPECT_FALSE(is_complete);
  Envoy::Buffer::OwnedImpl buf(std::string(1, 'a'));
  decoder->decodeData(buf, false);
  EXPECT_FALSE(is_complete);
  decoder->decodeData(buf, true);
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, TrailerTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete]() { is_complete = true; },
      connect_statistic_, latency_statistic_, request_headers_, false, 0, "654", http_tracer_);
  Envoy::Http::HeaderMapPtr headers{new Envoy::Http::TestHeaderMapImpl{{":status", "200"}}};
  decoder->decodeHeaders(std::move(headers), false);
  auto trailers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeTrailers(std::move(trailers));
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, LatencyIsNotMeasured) {
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, []() {}, connect_statistic_, latency_statistic_,
      request_headers_, false, 0, "654", http_tracer_);
  Envoy::Http::MockStreamEncoder stream_encoder;
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  EXPECT_CALL(stream_encoder, encodeHeaders(HeaderMapEqualRef(&request_headers_), true));
  decoder->onPoolReady(stream_encoder, ptr);
  Envoy::Http::HeaderMapPtr headers{new Envoy::Http::TestHeaderMapImpl{{":status", "200"}}};
  decoder->decodeHeaders(std::move(headers), true);
  EXPECT_EQ(0, connect_statistic_.count());
  EXPECT_EQ(0, latency_statistic_.count());
}

TEST_F(StreamDecoderTest, LatencyIsMeasured) {
  Envoy::Http::TestHeaderMapImpl request_header{{":method", "GET"}, {":path", "/"}};
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, []() {}, connect_statistic_, latency_statistic_,
      request_header, true, 0, "654", http_tracer_);
  Envoy::Http::MockStreamEncoder stream_encoder;
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  Envoy::Http::HeaderMapPtr expected_request_header{new Envoy::Http::TestHeaderMapImpl{
      {":method", "GET"}, {":path", "/"}, {"x-client-trace-id", "654"}}};
  EXPECT_CALL(stream_encoder,
              encodeHeaders(Envoy::HeaderMapEqualRef(expected_request_header.get()), true));
  decoder->onPoolReady(stream_encoder, ptr);
  EXPECT_EQ(1, connect_statistic_.count());
  Envoy::Http::HeaderMapPtr headers{new Envoy::Http::TestHeaderMapImpl{{":status", "200"}}};
  decoder->decodeHeaders(std::move(headers), true);
  EXPECT_EQ(1, connect_statistic_.count());
  EXPECT_EQ(1, latency_statistic_.count());
}

TEST_F(StreamDecoderTest, StreamResetTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete]() { is_complete = true; },
      connect_statistic_, latency_statistic_, request_headers_, false, 0, "654", http_tracer_);
  Envoy::Http::HeaderMapPtr headers{new Envoy::Http::TestHeaderMapImpl{{":status", "200"}}};
  decoder->decodeHeaders(std::move(headers), false);
  auto trailers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->onResetStream(Envoy::Http::StreamResetReason::LocalReset, "fooreason");
  // XXX(oschaaf):
  EXPECT_TRUE(is_complete); // these do get reported.
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, PoolFailureTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete]() { is_complete = true; },
      connect_statistic_, latency_statistic_, request_headers_, false, 0, "654", http_tracer_);
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  decoder->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::Overflow, "fooreason",
                         ptr);
  EXPECT_EQ(1, pool_failures_);
}

} // namespace Client
} // namespace Nighthawk
