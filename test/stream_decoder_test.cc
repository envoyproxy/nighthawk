#include <chrono>

#include "common/api/api_impl.h"
#include "common/common/thread_impl.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/http/header_map_impl.h"
#include "common/network/utility.h"
#include "common/runtime/runtime_impl.h"
#include "common/statistic_impl.h"
#include "common/stats/isolated_store_impl.h"

#include "client/stream_decoder.h"

#include "test/mocks.h"
#include "test/mocks/http/mocks.h"
#include "test/test_common/simulated_time_system.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

class StreamDecoderTest : public Test, public StreamDecoderCompletionCallback {
public:
  StreamDecoderTest() : api_(thread_factory_, store_, time_system_, file_system_) {}

  void onComplete(bool, const Envoy::Http::HeaderMap&) override {
    stream_decoder_completion_callbacks_++;
  }
  void onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason) override { pool_failures_++; }

  Envoy::Thread::ThreadFactoryImplPosix thread_factory_;
  Envoy::Event::SimulatedTimeSystem time_system_;
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Api::Impl api_;
  StreamingStatistic connect_statistic_;
  StreamingStatistic latency_statistic_;
  Envoy::Http::HeaderMapImpl request_headers_;
  Envoy::Filesystem::InstanceImplPosix file_system_;
  uint64_t stream_decoder_completion_callbacks_{0};
  uint64_t pool_failures_{0};
};

TEST_F(StreamDecoderTest, HeaderOnlyTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      time_system_, *this, [&is_complete]() { is_complete = true; }, connect_statistic_,
      latency_statistic_, request_headers_, false, 0);
  auto headers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeHeaders(std::move(headers), true);
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, HeaderWithBodyTest) {
  bool is_complete = false;
  Envoy::Http::MockStreamEncoder stream_encoder;
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  EXPECT_CALL(stream_encoder, encodeHeaders(HeaderMapEqualRef(&request_headers_), true));
  auto decoder = new StreamDecoder(
      time_system_, *this, [&is_complete]() { is_complete = true; }, connect_statistic_,
      latency_statistic_, request_headers_, true, 0);
  time_system_.sleep(1s);
  decoder->onPoolReady(stream_encoder, ptr);
  time_system_.sleep(1s);
  auto headers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeHeaders(std::move(headers), false);
  EXPECT_FALSE(is_complete);
  Envoy::Buffer::OwnedImpl buf(std::string(1, 'a'));
  time_system_.sleep(1s);
  decoder->decodeData(buf, false);
  EXPECT_FALSE(is_complete);
  time_system_.sleep(1s);
  decoder->decodeData(buf, true);
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
  EXPECT_EQ(1, connect_statistic_.count());
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::nanoseconds>(1s).count(),
            connect_statistic_.mean());
  EXPECT_EQ(1, latency_statistic_.count());
  EXPECT_EQ(std::chrono::duration_cast<std::chrono::nanoseconds>(3s).count(),
            latency_statistic_.mean());
}

TEST_F(StreamDecoderTest, TrailerTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      time_system_, *this, [&is_complete]() { is_complete = true; }, connect_statistic_,
      latency_statistic_, request_headers_, false, 0);
  auto headers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeHeaders(std::move(headers), false);
  auto trailers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeTrailers(std::move(trailers));
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, LatencyIsNotMeasured) {
  auto decoder = new StreamDecoder(
      time_system_, *this, []() {}, connect_statistic_, latency_statistic_, request_headers_, false,
      0);
  Envoy::Http::MockStreamEncoder stream_encoder;
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  EXPECT_CALL(stream_encoder, encodeHeaders(HeaderMapEqualRef(&request_headers_), true));
  decoder->onPoolReady(stream_encoder, ptr);
  auto headers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeHeaders(std::move(headers), true);
  EXPECT_EQ(0, connect_statistic_.count());
  EXPECT_EQ(0, latency_statistic_.count());
}

TEST_F(StreamDecoderTest, LatencyIsMeasured) {
  auto decoder = new StreamDecoder(
      time_system_, *this, []() {}, connect_statistic_, latency_statistic_, request_headers_, true,
      0);
  Envoy::Http::MockStreamEncoder stream_encoder;
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  EXPECT_CALL(stream_encoder, encodeHeaders(HeaderMapEqualRef(&request_headers_), true));
  decoder->onPoolReady(stream_encoder, ptr);
  EXPECT_EQ(1, connect_statistic_.count());
  auto headers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeHeaders(std::move(headers), true);
  EXPECT_EQ(1, connect_statistic_.count());
  EXPECT_EQ(1, latency_statistic_.count());
}

TEST_F(StreamDecoderTest, StreamResetTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      time_system_, *this, [&is_complete]() { is_complete = true; }, connect_statistic_,
      latency_statistic_, request_headers_, false, 0);
  auto headers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeHeaders(std::move(headers), false);
  decoder->onResetStream(Envoy::Http::StreamResetReason::LocalReset, "fooreason");
  EXPECT_FALSE(is_complete); // these do not get reported.
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, PoolFailureTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      time_system_, *this, [&is_complete]() { is_complete = true; }, connect_statistic_,
      latency_statistic_, request_headers_, false, 0);
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  decoder->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::Overflow, "fooreason",
                         ptr);
  EXPECT_EQ(1, pool_failures_);
}

} // namespace Client
} // namespace Nighthawk
