#include <string>

#include "external/envoy/test/integration/http_integration.h"

#include "api/server/response_options.pb.h"
#include "api/server/response_options.pb.validate.h"

#include "server/common.h"
#include "server/http_dynamic_delay_filter.h"

#include "gtest/gtest.h"

namespace Nighthawk {

using namespace testing;

class HttpDynamicDelayIntegrationTest
    : public Envoy::HttpIntegrationTest,
      public testing::TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  HttpDynamicDelayIntegrationTest()
      : HttpIntegrationTest(Envoy::Http::CodecClient::Type::HTTP1, GetParam()),
        request_headers_({{":method", "GET"}, {":path", "/"}, {":authority", "host"}}),
        delay_header_string_(Envoy::Http::LowerCaseString("x-envoy-fault-delay-request")) {}

  void setup(const std::string& config) {
    config_helper_.addFilter(config);
    HttpIntegrationTest::initialize();
  }

  void TearDown() override { cleanupUpstreamAndDownstream(); }

  Envoy::IntegrationStreamDecoderPtr getResponse(absl::string_view request_level_config) {
    const Envoy::Http::LowerCaseString key("x-nighthawk-test-server-config");
    request_headers_.setCopy(key, request_level_config);
    return getResponse();
  }

  Envoy::IntegrationStreamDecoderPtr getResponse() {
    cleanupUpstreamAndDownstream();
    codec_client_ = makeHttpConnection(lookupPort("http"));
    auto response =
        sendRequestAndWaitForResponse(request_headers_, 0, default_response_headers_, 0);
    return response;
  }

  Envoy::Http::TestRequestHeaderMapImpl request_headers_;
  const Envoy::Http::LowerCaseString delay_header_string_;
};

INSTANTIATE_TEST_SUITE_P(IpVersions, HttpDynamicDelayIntegrationTest,
                         testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

TEST_P(HttpDynamicDelayIntegrationTest, NoStaticConfiguration) {
  setup(R"(
name: dynamic-delay
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
)");
  // Don't send any config request header
  getResponse();
  EXPECT_EQ(upstream_request_->headers().get(delay_header_string_), nullptr);
  // Send a config request header with an empty / default config. Should be a no-op.
  getResponse("{}");
  EXPECT_EQ(upstream_request_->headers().get(delay_header_string_), nullptr);
  // Send a config request header, this should become effective.
  getResponse("{static_delay: \"1.6s\"}");
  EXPECT_EQ(upstream_request_->headers().get(delay_header_string_)->value().getStringView(),
            "1600");
}

TEST_P(HttpDynamicDelayIntegrationTest, StaticConfigurationStaticDelay) {
  setup(R"EOF(
name: dynamic-delay
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  static_delay: 1.33s
)EOF");
  getResponse();
  EXPECT_EQ(upstream_request_->headers().get(delay_header_string_)->value().getStringView(),
            "1330");
  getResponse("{}");
  EXPECT_EQ(upstream_request_->headers().get(delay_header_string_)->value().getStringView(),
            "1330");
  getResponse("{static_delay: \"0.2s\"}");
  // TODO(#392): This fails, because the duration is a two-field message: it would make here to see
  // both the number of seconds and nanoseconds to be overridden.
  // However, the seconds part is set to '0', which equates to the default of the underlying int
  // type, and the fact that we are using proto3, which doesn't merge default values.
  // Hence the following expectation will fail, as it yields 1200 instead of the expected 200.
  // EXPECT_EQ(upstream_request_->headers().get(delay_header_string_)->value().getStringView(),
  // "200");
  getResponse("{static_delay: \"2.2s\"}");
  EXPECT_EQ(upstream_request_->headers().get(delay_header_string_)->value().getStringView(),
            "2200");
}

TEST_P(HttpDynamicDelayIntegrationTest, StaticConfigurationConcurrentDelay) {
  setup(R"EOF(
name: dynamic-delay
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  concurrency_based_delay:
    minimal_delay: 0.05s
    concurrency_delay_factor: 0.01s
)EOF");
  getResponse();
  EXPECT_EQ(upstream_request_->headers().get(delay_header_string_)->value().getStringView(), "60");
}

class ComputeTest : public testing::Test {
public:
  int64_t compute(uint64_t concurrency, uint64_t minimal_delay_seconds,
                  uint64_t minimal_delay_nanos, uint64_t delay_factor_seconds,
                  uint64_t delay_factor_nanos) {
    Envoy::ProtobufWkt::Duration minimal_delay;
    Envoy::ProtobufWkt::Duration delay_factor;
    minimal_delay.set_seconds(minimal_delay_seconds);
    minimal_delay.set_nanos(minimal_delay_nanos);
    delay_factor.set_seconds(delay_factor_seconds);
    delay_factor.set_nanos(delay_factor_nanos);
    return Server::HttpDynamicDelayDecoderFilter::computeDelayMilliseconds(
        concurrency, minimal_delay, delay_factor);
  }
};

TEST_F(ComputeTest, ComputeDelayMilliseconds) {
  EXPECT_EQ(compute(1, 1, 0, 0, 0), 1000);
  EXPECT_EQ(compute(2, 1, 0, 0, 0), 1000);
  EXPECT_EQ(compute(1, 2, 0, 0, 0), 2000);
  EXPECT_EQ(compute(2, 2, 0, 0, 0), 2000);
  EXPECT_EQ(compute(1, 0, 500000, 0, 500000), 1);
  EXPECT_EQ(compute(2, 0, 500000, 0, 500000), 2);
  EXPECT_EQ(compute(3, 0, 500000, 0, 500000), 2);
  EXPECT_EQ(compute(4, 0, 500000, 0, 500000), 3);
  EXPECT_EQ(compute(4, 1, 500000, 1, 500000), 5003);
}

} // namespace Nighthawk
