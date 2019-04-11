#include <chrono>

#include "gtest/gtest.h"

#include "common/api/api_impl.h"
#include "common/common/thread_impl.h"
#include "common/event/dispatcher_impl.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/http/header_map_impl.h"
#include "common/network/utility.h"
#include "common/runtime/runtime_impl.h"
#include "common/stats/isolated_store_impl.h"

#include "test/mocks/http/mocks.h"

#include "nighthawk/test/mocks.h"

#include "nighthawk/source/client/stream_decoder.h"
#include "nighthawk/source/common/statistic_impl.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

class StreamDecoderTest : public testing::Test, public StreamDecoderCompletionCallback {
public:
  StreamDecoderTest()
      : api_(thread_factory_, store_, time_system_, file_system_),
        dispatcher_(api_.allocateDispatcher()), stream_decoder_completion_callbacks_(0),
        pool_failures_(0) {}

  void onComplete(bool, const Envoy::Http::HeaderMap&) override {
    stream_decoder_completion_callbacks_++;
  }
  void onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason) override { pool_failures_++; }

  Envoy::Thread::ThreadFactoryImplPosix thread_factory_;
  Envoy::Event::RealTimeSystem time_system_;
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Api::Impl api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  StreamingStatistic connect_statistic_;
  StreamingStatistic latency_statistic_;
  Envoy::Http::HeaderMapImpl request_headers_;
  Envoy::Filesystem::InstanceImplPosix file_system_;
  uint64_t stream_decoder_completion_callbacks_;
  uint64_t pool_failures_;
};

TEST_F(StreamDecoderTest, HeaderOnlyTest) {
  bool is_complete = false;
  auto decoder =
      new StreamDecoder(*dispatcher_, time_system_, *this, [&is_complete]() { is_complete = true; },
                        connect_statistic_, latency_statistic_, request_headers_, false);
  auto headers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeHeaders(std::move(headers), true);
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, HeaderWithBodyTest) {
  bool is_complete = false;
  auto decoder =
      new StreamDecoder(*dispatcher_, time_system_, *this, [&is_complete]() { is_complete = true; },
                        connect_statistic_, latency_statistic_, request_headers_, false);
  auto headers = std::make_unique<Envoy::Http::HeaderMapImpl>();
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
  auto decoder =
      new StreamDecoder(*dispatcher_, time_system_, *this, [&is_complete]() { is_complete = true; },
                        connect_statistic_, latency_statistic_, request_headers_, false);
  auto headers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeHeaders(std::move(headers), false);
  auto trailers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeTrailers(std::move(trailers));
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, LatencyIsNotMeasured) {
  auto decoder = new StreamDecoder(*dispatcher_, time_system_, *this, []() {}, connect_statistic_,
                                   latency_statistic_, request_headers_, false);
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
  auto decoder = new StreamDecoder(*dispatcher_, time_system_, *this, []() {}, connect_statistic_,
                                   latency_statistic_, request_headers_, true);
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
  auto decoder =
      new StreamDecoder(*dispatcher_, time_system_, *this, [&is_complete]() { is_complete = true; },
                        connect_statistic_, latency_statistic_, request_headers_, false);
  auto headers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeHeaders(std::move(headers), false);
  auto trailers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->onResetStream(Envoy::Http::StreamResetReason::LocalReset, "fooreason");
  EXPECT_FALSE(is_complete); // these do not get reported.
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, PoolFailureTest) {
  bool is_complete = false;
  auto decoder =
      new StreamDecoder(*dispatcher_, time_system_, *this, [&is_complete]() { is_complete = true; },
                        connect_statistic_, latency_statistic_, request_headers_, false);
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  decoder->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::Overflow, "fooreason",
                         ptr);
  EXPECT_EQ(1, pool_failures_);
}

} // namespace Client
} // namespace Nighthawk
