#include <chrono>

#include "external/envoy/source/common/common/random_generator.h"
#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/test/mocks/http/mocks.h"
#include "external/envoy/test/mocks/stream_info/mocks.h"

#include "common/statistic_impl.h"

#include "client/stream_decoder.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

class StreamDecoderTest : public Test, public StreamDecoderCompletionCallback {
public:
  StreamDecoderTest()
      : api_(Envoy::Api::createApiForTest(time_system_)),
        dispatcher_(api_->allocateDispatcher("test_thread")),
        request_headers_(std::make_shared<Envoy::Http::TestRequestHeaderMapImpl>(
            std::initializer_list<std::pair<std::string, std::string>>({{":method", "GET"}}))),
        http_tracer_(std::make_unique<Envoy::Tracing::HttpNullTracer>()),
        test_header_(std::make_unique<Envoy::Http::TestResponseHeaderMapImpl>(
            std::initializer_list<std::pair<std::string, std::string>>({{":status", "200"}}))),
        test_trailer_(std::make_unique<Envoy::Http::TestResponseTrailerMapImpl>(
            std::initializer_list<std::pair<std::string, std::string>>({{}}))) {}

  void onComplete(bool, const Envoy::Http::ResponseHeaderMap&) override {
    stream_decoder_completion_callbacks_++;
  }
  void onPoolFailure(Envoy::Http::ConnectionPool::PoolFailureReason) override { pool_failures_++; }
  void exportLatency(const uint32_t, const uint64_t) override {
    stream_decoder_export_latency_callbacks_++;
  }

  Envoy::Event::TestRealTimeSystem time_system_;
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Api::ApiPtr api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  StreamingStatistic connect_statistic_;
  StreamingStatistic latency_statistic_;
  StreamingStatistic response_header_size_statistic_;
  StreamingStatistic response_body_size_statistic_;
  StreamingStatistic origin_latency_statistic_;
  HeaderMapPtr request_headers_;
  uint64_t stream_decoder_completion_callbacks_{0};
  uint64_t pool_failures_{0};
  uint64_t stream_decoder_export_latency_callbacks_{0};
  Envoy::Random::RandomGeneratorImpl random_generator_;
  Envoy::Tracing::HttpTracerSharedPtr http_tracer_;
  Envoy::Http::ResponseHeaderMapPtr test_header_;
  Envoy::Http::ResponseTrailerMapPtr test_trailer_;
};

TEST_F(StreamDecoderTest, HeaderOnlyTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete](bool, bool) { is_complete = true; },
      connect_statistic_, latency_statistic_, response_header_size_statistic_,
      response_body_size_statistic_, origin_latency_statistic_, request_headers_, false, 0,
      random_generator_, http_tracer_, "");
  decoder->decodeHeaders(std::move(test_header_), true);
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
  EXPECT_EQ(0, stream_decoder_export_latency_callbacks_);
}

TEST_F(StreamDecoderTest, HeaderWithBodyTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete](bool, bool) { is_complete = true; },
      connect_statistic_, latency_statistic_, response_header_size_statistic_,
      response_body_size_statistic_, origin_latency_statistic_, request_headers_, false, 0,
      random_generator_, http_tracer_, "");
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
      connect_statistic_, latency_statistic_, response_header_size_statistic_,
      response_body_size_statistic_, origin_latency_statistic_, request_headers_, false, 0,
      random_generator_, http_tracer_, "");
  Envoy::Http::ResponseHeaderMapPtr headers{
      new Envoy::Http::TestResponseHeaderMapImpl{{":status", "200"}}};
  decoder->decodeHeaders(std::move(headers), false);
  Envoy::Http::ResponseTrailerMapPtr trailers = Envoy::Http::ResponseTrailerMapImpl::create();
  decoder->decodeTrailers(std::move(trailers));
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
}

TEST_F(StreamDecoderTest, LatencyIsNotMeasured) {
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [](bool, bool) {}, connect_statistic_, latency_statistic_,
      response_header_size_statistic_, response_body_size_statistic_, origin_latency_statistic_,
      request_headers_, false, 0, random_generator_, http_tracer_, "");
  Envoy::Http::MockRequestEncoder stream_encoder;
  EXPECT_CALL(stream_encoder, getStream());
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info;
  EXPECT_CALL(stream_encoder,
              encodeHeaders(Envoy::HeaderMapEqualRef(request_headers_.get()), true));
  decoder->onPoolReady(stream_encoder, ptr, stream_info);
  decoder->decodeHeaders(std::move(test_header_), true);
  EXPECT_EQ(0, connect_statistic_.count());
  EXPECT_EQ(0, latency_statistic_.count());
  EXPECT_EQ(0, stream_decoder_export_latency_callbacks_);
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
            EXPECT_CALL(*span, setTag(_, _)).Times(12);
            EXPECT_CALL(*span, finishSpan()).Times(1);
            return span;
          }));

  auto request_header = std::make_shared<Envoy::Http::TestRequestHeaderMapImpl>(
      std::initializer_list<std::pair<std::string, std::string>>(
          {{":method", "GET"}, {":path", "/"}}));
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [](bool, bool) {}, connect_statistic_, latency_statistic_,
      response_header_size_statistic_, response_body_size_statistic_, origin_latency_statistic_,
      request_header, true, 0, random_generator_, http_tracer_, "");
  Envoy::Http::MockRequestEncoder stream_encoder;
  EXPECT_CALL(stream_encoder, getStream());
  Envoy::Upstream::HostDescriptionConstSharedPtr ptr;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info;
  EXPECT_CALL(stream_encoder, encodeHeaders(_, true));
  decoder->onPoolReady(stream_encoder, ptr, stream_info);
  EXPECT_EQ(1, connect_statistic_.count());
  decoder->decodeHeaders(std::move(test_header_), false);
  EXPECT_EQ(0, stream_decoder_export_latency_callbacks_);
  decoder->decodeTrailers(std::move(test_trailer_));
  EXPECT_EQ(1, connect_statistic_.count());
  EXPECT_EQ(1, latency_statistic_.count());
  EXPECT_EQ(1, stream_decoder_export_latency_callbacks_);
}

TEST_F(StreamDecoderTest, StreamResetTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete](bool, bool) { is_complete = true; },
      connect_statistic_, latency_statistic_, response_header_size_statistic_,
      response_body_size_statistic_, origin_latency_statistic_, request_headers_, false, 0,
      random_generator_, http_tracer_, "");
  decoder->decodeHeaders(std::move(test_header_), false);
  decoder->onResetStream(Envoy::Http::StreamResetReason::LocalReset, "fooreason");
  EXPECT_TRUE(is_complete); // these do get reported.
  EXPECT_EQ(1, stream_decoder_completion_callbacks_);
  EXPECT_EQ(0, stream_decoder_export_latency_callbacks_);
}

TEST_F(StreamDecoderTest, PoolFailureTest) {
  bool is_complete = false;
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [&is_complete](bool, bool) { is_complete = true; },
      connect_statistic_, latency_statistic_, response_header_size_statistic_,
      response_body_size_statistic_, origin_latency_statistic_, request_headers_, false, 0,
      random_generator_, http_tracer_, "");
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
  ASSERT_EQ(
      StreamDecoder::streamResetReasonToResponseFlag(Envoy::Http::StreamResetReason::ConnectError),
      Envoy::StreamInfo::ResponseFlag::UpstreamRemoteReset);
}

// This test parameterization structure carries the response header name that ought to be treated
// as a latency input that should be tracked, as well as a boolean indicating if we ought to expect
// the latency delivered via that header to be added to the histogram.
using LatencyTrackingViaResponseHeaderTestParam = std::tuple<const char*, bool>;

class LatencyTrackingViaResponseHeaderTest
    : public StreamDecoderTest,
      public WithParamInterface<LatencyTrackingViaResponseHeaderTestParam> {};

INSTANTIATE_TEST_SUITE_P(ResponseHeaderLatencies, LatencyTrackingViaResponseHeaderTest,
                         ValuesIn({LatencyTrackingViaResponseHeaderTestParam{"0", true},
                                   LatencyTrackingViaResponseHeaderTestParam{"1", true},
                                   LatencyTrackingViaResponseHeaderTestParam{"-1", false},
                                   LatencyTrackingViaResponseHeaderTestParam{"1000", true},
                                   LatencyTrackingViaResponseHeaderTestParam{"invalid", false},
                                   LatencyTrackingViaResponseHeaderTestParam{"", false}}));

// Tests that the StreamDecoder handles delivery of latencies by response header.
TEST_P(LatencyTrackingViaResponseHeaderTest, LatencyTrackingViaResponseHeader) {
  const std::string kLatencyTrackingResponseHeader = "latency-in-response-header";
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [](bool, bool) {}, connect_statistic_, latency_statistic_,
      response_header_size_statistic_, response_body_size_statistic_, origin_latency_statistic_,
      request_headers_, false, 0, random_generator_, http_tracer_, kLatencyTrackingResponseHeader);
  const LatencyTrackingViaResponseHeaderTestParam param = GetParam();
  Envoy::Http::ResponseHeaderMapPtr headers{new Envoy::Http::TestResponseHeaderMapImpl{
      {":status", "200"}, {kLatencyTrackingResponseHeader, std::get<0>(param)}}};
  decoder->decodeHeaders(std::move(headers), true);
  const uint64_t expected_count = std::get<1>(param) ? 1 : 0;
  EXPECT_EQ(origin_latency_statistic_.count(), expected_count);
}

// Test that a single response carrying multiple valid latency response headers does not
// get tracked. This will also yield a burst of warnings, which we unfortunately cannot
// easily verify here.
TEST_F(StreamDecoderTest, LatencyTrackingWithMultipleResponseHeadersFails) {
  const std::string kLatencyTrackingResponseHeader = "latency-in-response-header";
  auto decoder = new StreamDecoder(
      *dispatcher_, time_system_, *this, [](bool, bool) {}, connect_statistic_, latency_statistic_,
      response_header_size_statistic_, response_body_size_statistic_, origin_latency_statistic_,
      request_headers_, false, 0, random_generator_, http_tracer_, kLatencyTrackingResponseHeader);
  Envoy::Http::ResponseHeaderMapPtr headers{
      new Envoy::Http::TestResponseHeaderMapImpl{{":status", "200"},
                                                 {kLatencyTrackingResponseHeader, "1"},
                                                 {kLatencyTrackingResponseHeader, "2"}}};
  decoder->decodeHeaders(std::move(headers), true);
  EXPECT_EQ(origin_latency_statistic_.count(), 0);
}

} // namespace Client
} // namespace Nighthawk
