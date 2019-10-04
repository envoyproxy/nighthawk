#include <chrono>

#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/test/mocks/http/mocks.h"
#include "external/envoy/test/mocks/stream_info/mocks.h"

#include "common/statistic_impl.h"

#include "client/stream_decoder.h"

#include "test/mocks.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

namespace {
static const std::string TEST_TRACER_UID = "f4dca0a9-12c7-4307-8002-969403baf480";
static const std::string TEST_TRACER_UID_BIT_SET = "f4dca0a9-12c7-b307-8002-969403baf480";
} // namespace

class StreamDecoderTest : public Test, public StreamDecoderCompletionCallback {
public:
  StreamDecoderTest()
      : api_(Envoy::Api::createApiForTest()), dispatcher_(api_->allocateDispatcher()),
        request_headers_(std::make_shared<Envoy::Http::HeaderMapImpl>()),
        http_tracer_(std::make_unique<Envoy::Tracing::HttpNullTracer>()),
        test_header_(std::make_unique<Envoy::Http::TestHeaderMapImpl>(
            std::initializer_list<std::pair<std::string, std::string>>({{":status", "200"}}))) {}

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
  HeaderMapPtr request_headers_;
  uint64_t stream_decoder_completion_callbacks_{0};
  uint64_t pool_failures_{0};
  Envoy::Tracing::HttpTracerPtr http_tracer_;
  Envoy::Http::HeaderMapPtr test_header_;
};

TEST_F(StreamDecoderTest, HeaderOnlyTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete](bool, bool) { is_complete = true; },
      connect_statistic_, latency_statistic_, request_headers_, false, 0, TEST_TRACER_UID,
      http_tracer_);
  decoder->decodeHeaders(std::move(test_header_), true);
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, HeaderWithBodyTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete](bool, bool) { is_complete = true; },
      connect_statistic_, latency_statistic_, request_headers_, false, 0, TEST_TRACER_UID,
      http_tracer_);
  decoder->decodeHeaders(std::move(test_header_), false);
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
      *dispatcher_, time_system_, *this, [&is_complete](bool, bool) { is_complete = true; },
      connect_statistic_, latency_statistic_, request_headers_, false, 0, TEST_TRACER_UID,
      http_tracer_);
  Envoy::Http::HeaderMapPtr headers{new Envoy::Http::TestHeaderMapImpl{{":status", "200"}}};
  decoder->decodeHeaders(std::move(headers), false);
  auto trailers = std::make_unique<Envoy::Http::HeaderMapImpl>();
  decoder->decodeTrailers(std::move(trailers));
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, LatencyIsNotMeasured) {
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [](bool, bool) {}, connect_statistic_, latency_statistic_,
      request_headers_, false, 0, TEST_TRACER_UID, http_tracer_);
  Envoy::Http::MockStreamEncoder stream_encoder;
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info;
  EXPECT_CALL(stream_encoder,
              encodeHeaders(Envoy::HeaderMapEqualRef(request_headers_.get()), true));
  decoder->onPoolReady(stream_encoder, ptr, stream_info);
  decoder->decodeHeaders(std::move(test_header_), true);
  EXPECT_EQ(0, connect_statistic_.count());
  EXPECT_EQ(0, latency_statistic_.count());
}

TEST_F(StreamDecoderTest, LatencyIsMeasured) {
  http_tracer_ = std::make_unique<Envoy::Tracing::MockHttpTracer>();
  EXPECT_CALL(*dynamic_cast<Envoy::Tracing::MockHttpTracer*>(http_tracer_.get()),
              startSpan_(_, _, _, _))
      .WillRepeatedly(
          Invoke([&](const Envoy::Tracing::Config& config, const Envoy::Http::HeaderMap&,
                     const Envoy::StreamInfo::StreamInfo&,
                     const Envoy::Tracing::Decision) -> Envoy::Tracing::Span* {
            EXPECT_EQ(Envoy::Tracing::OperationName::Egress, config.operationName());
            auto* span = new Envoy::Tracing::MockSpan();
            EXPECT_CALL(*span, injectContext(_)).Times(1);
            EXPECT_CALL(*span, setTag(_, _)).Times(11);
            EXPECT_CALL(*span, finishSpan()).Times(1);
            return span;
          }));

  auto request_header = std::make_shared<Envoy::Http::TestHeaderMapImpl>(
      std::initializer_list<std::pair<std::string, std::string>>(
          {{":method", "GET"}, {":path", "/"}}));
  auto expected_request_header = std::make_shared<Envoy::Http::TestHeaderMapImpl>(
      std::initializer_list<std::pair<std::string, std::string>>(
          {{":method", "GET"}, {":path", "/"}, {"x-client-trace-id", TEST_TRACER_UID_BIT_SET}}));
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [](bool, bool) {}, connect_statistic_, latency_statistic_,
      request_header, true, 0, TEST_TRACER_UID, http_tracer_);

  Envoy::Http::MockStreamEncoder stream_encoder;
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info;
  EXPECT_CALL(stream_encoder,
              encodeHeaders(Envoy::HeaderMapEqualRef(expected_request_header.get()), true));
  decoder->onPoolReady(stream_encoder, ptr, stream_info);
  EXPECT_EQ(1, connect_statistic_.count());
  decoder->decodeHeaders(std::move(test_header_), false);
  decoder->decodeTrailers(std::move(test_header_));
  EXPECT_EQ(1, connect_statistic_.count());
  EXPECT_EQ(1, latency_statistic_.count());
}

TEST_F(StreamDecoderTest, StreamResetTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete](bool, bool) { is_complete = true; },
      connect_statistic_, latency_statistic_, request_headers_, false, 0, TEST_TRACER_UID,
      http_tracer_);
  decoder->decodeHeaders(std::move(test_header_), false);
  decoder->onResetStream(Envoy::Http::StreamResetReason::LocalReset, "fooreason");
  EXPECT_TRUE(is_complete); // these do get reported.
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, PoolFailureTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete](bool, bool) { is_complete = true; },
      connect_statistic_, latency_statistic_, request_headers_, false, 0, TEST_TRACER_UID,
      http_tracer_);
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  decoder->onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason::Overflow, "fooreason",
                         ptr);
  EXPECT_EQ(1, pool_failures_);
}

TEST_F(StreamDecoderTest, StreamResetReasonToResponseFlag) {
  ASSERT_EQ(StreamDecoder::streamResetReasonToResponseFlag(
                Envoy::Http::StreamResetReason::ConnectionFailure),
            Envoy::StreamInfo::ResponseFlag::UpstreamConnectionFailure);
  ASSERT_EQ(StreamDecoder::streamResetReasonToResponseFlag(
                Envoy::Http::StreamResetReason::ConnectionTermination),
            Envoy::StreamInfo::ResponseFlag::UpstreamConnectionTermination);
  ASSERT_EQ(
      StreamDecoder::streamResetReasonToResponseFlag(Envoy::Http::StreamResetReason::LocalReset),
      Envoy::StreamInfo::ResponseFlag::LocalReset);
  ASSERT_EQ(StreamDecoder::streamResetReasonToResponseFlag(
                Envoy::Http::StreamResetReason::LocalRefusedStreamReset),
            Envoy::StreamInfo::ResponseFlag::LocalReset);
  ASSERT_EQ(
      StreamDecoder::streamResetReasonToResponseFlag(Envoy::Http::StreamResetReason::Overflow),
      Envoy::StreamInfo::ResponseFlag::UpstreamOverflow);
  ASSERT_EQ(
      StreamDecoder::streamResetReasonToResponseFlag(Envoy::Http::StreamResetReason::RemoteReset),
      Envoy::StreamInfo::ResponseFlag::UpstreamRemoteReset);
  ASSERT_EQ(StreamDecoder::streamResetReasonToResponseFlag(
                Envoy::Http::StreamResetReason::RemoteRefusedStreamReset),
            Envoy::StreamInfo::ResponseFlag::UpstreamRemoteReset);
}

} // namespace Client
} // namespace Nighthawk
