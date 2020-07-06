#include <string>

#include "external/envoy/test/integration/http_integration.h"

#include "api/server/response_options.pb.h"

#include "server/common.h"
#include "server/http_dynamic_delay_filter.h"

#include "gtest/gtest.h"

namespace Nighthawk {

/**
 * Support class for testing the dynamic delay filter. We rely on the fault filter for
 * inducing the actual delay, so this aims to prove that:
 * - The computations are correct.
 * - Static/file-based configuration is handled as expected.
 * - Request level configuration is handled as expected.
 * - Failure modes work.
 * - TODO(#393): An end to end test which proves that the interaction between this filter
 *   and the fault filter work as expected.
 */
class HttpDynamicDelayIntegrationTest
    : public Envoy::HttpIntegrationTest,
      public testing::TestWithParam<Envoy::Network::Address::IpVersion> {
protected:
  HttpDynamicDelayIntegrationTest()
      : HttpIntegrationTest(Envoy::Http::CodecClient::Type::HTTP1, GetParam()),
        request_headers_({{":method", "GET"}, {":path", "/"}, {":authority", "host"}}),
        delay_header_string_(Envoy::Http::LowerCaseString("x-envoy-fault-delay-request")) {}

  // We don't override SetUp(): tests in this file will call setup() instead to avoid having to
  // create a fixture per filter configuration.
  void setup(const std::string& config) {
    config_helper_.addFilter(config);
    HttpIntegrationTest::initialize();
  }

  // Fetches a response with request-level configuration set in the request header.
  Envoy::IntegrationStreamDecoderPtr getResponse(absl::string_view request_level_config,
                                                 bool setup_for_upstream_request = true) {
    const Envoy::Http::LowerCaseString key("x-nighthawk-test-server-config");
    Envoy::Http::TestRequestHeaderMapImpl request_headers = request_headers_;
    request_headers.setCopy(key, request_level_config);
    return getResponse(request_headers, setup_for_upstream_request);
  }

  // Fetches a response with the default request headers, expecting the fake upstream to supply
  // the response.
  Envoy::IntegrationStreamDecoderPtr getResponse() { return getResponse(request_headers_); }

  // Fetches a response using the provided request headers. When setup_for_upstream_request
  // is true, the expectation will be that an upstream request will be needed to provide a
  // response. If it is set to false, the extension is expected to supply the response, and
  // no upstream request ought to occur.
  Envoy::IntegrationStreamDecoderPtr
  getResponse(const Envoy::Http::TestRequestHeaderMapImpl& request_headers,
              bool setup_for_upstream_request = true) {
    cleanupUpstreamAndDownstream();
    codec_client_ = makeHttpConnection(lookupPort("http"));
    Envoy::IntegrationStreamDecoderPtr response;
    if (setup_for_upstream_request) {
      response = sendRequestAndWaitForResponse(request_headers, 0, default_response_headers_, 0);
    } else {
      response = codec_client_->makeHeaderOnlyRequest(request_headers);
      response->waitForEndStream();
    }
    return response;
  }

  const Envoy::Http::TestRequestHeaderMapImpl request_headers_;
  const Envoy::Http::LowerCaseString delay_header_string_;
};

INSTANTIATE_TEST_SUITE_P(IpVersions, HttpDynamicDelayIntegrationTest,
                         testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

// Verify expectations with an empty dynamic-delay configuration.
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

  // Send a malformed config request header. This ought to shortcut and directly reply,
  // hence we don't expect an upstream request.
  auto response = getResponse("bad_json", false);
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_EQ(
      response->body(),
      "dynamic-delay didn't understand the request: Error merging json config: Unable to parse "
      "JSON as proto (INVALID_ARGUMENT:Unexpected token.\nbad_json\n^): bad_json");
  // Send an empty config header, which ought to trigger failure mode as well.
  response = getResponse("", false);
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_EQ(
      response->body(),
      "dynamic-delay didn't understand the request: Error merging json config: Unable to "
      "parse JSON as proto (INVALID_ARGUMENT:Unexpected end of string. Expected a value.\n\n^): ");
}

// Verify expectations with static/file-based static_delay configuration.
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

// Verify expectations with static/file-based concurrency_based_linear_delay configuration.
TEST_P(HttpDynamicDelayIntegrationTest, StaticConfigurationConcurrentDelay) {
  setup(R"EOF(
name: dynamic-delay
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  concurrency_based_linear_delay:
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

// Test that the delay looks as expected with various parameterizations.
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
