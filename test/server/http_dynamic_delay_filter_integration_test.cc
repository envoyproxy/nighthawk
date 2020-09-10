#include <string>

#include "api/server/response_options.pb.h"

#include "server/configuration.h"
#include "server/http_dynamic_delay_filter.h"

#include "test/server/http_filter_integration_test_base.h"

#include "gtest/gtest.h"

namespace Nighthawk {

const Envoy::Http::LowerCaseString kDelayHeaderString("x-envoy-fault-delay-request");

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
    : public HttpFilterIntegrationTestBase,
      public testing::TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  HttpDynamicDelayIntegrationTest() : HttpFilterIntegrationTestBase(GetParam()){};
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
  getResponseFromUpstream();
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString), nullptr);
  // Send a config request header with an empty / default config. Should be a no-op.
  getResponseFromUpstream("{}");
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString), nullptr);
  // Send a config request header, this should become effective.
  getResponseFromUpstream("{static_delay: \"1.6s\"}");
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)->value().getStringView(), "1600");

  // Send a malformed config request header. This ought to shortcut and directly reply,
  // hence we don't expect an upstream request.
  auto response = getResponseFromExtension("bad_json");
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_EQ(
      response->body(),
      "dynamic-delay didn't understand the request: Error merging json config: Unable to parse "
      "JSON as proto (INVALID_ARGUMENT:Unexpected token.\nbad_json\n^): bad_json");
  // Send an empty config header, which ought to trigger failure mode as well.
  response = getResponseFromExtension("");
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_EQ(
      response->body(),
      "dynamic-delay didn't understand the request: Error merging json config: Unable to "
      "parse JSON as proto (INVALID_ARGUMENT:Unexpected end of string. Expected a value.\n\n^): ");
}

// Verify the filter is well-behaved when it comes to requests with an entity body.
// This takes a slightly different code path, so it is important to test this explicitly.
TEST_P(HttpDynamicDelayIntegrationTest, BehaviorWithRequestBody) {
  setup(R"EOF(
name: dynamic-delay
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  static_delay: 0.1s
)EOF");
  Envoy::Http::TestRequestHeaderMapImpl request_headers(
      {{":method", "POST"}, {":path", "/"}, {":authority", "host"}});

  // Post without any request-level configuration. Should succeed.
  getResponseFromUpstream(request_headers);
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)->value().getStringView(), "100");

  // Post with bad request-level configuration. The extension should response directly with an
  // error.
  const Envoy::Http::LowerCaseString key("x-nighthawk-test-server-config");
  request_headers.setCopy(key, "bad_json");
  auto response = getResponseFromExtension(request_headers);
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_EQ(
      response->body(),
      "dynamic-delay didn't understand the request: Error merging json config: Unable to parse "
      "JSON as proto (INVALID_ARGUMENT:Unexpected token.\nbad_json\n^): bad_json");
}

// Verify expectations with static/file-based static_delay configuration.
TEST_P(HttpDynamicDelayIntegrationTest, StaticConfigurationStaticDelay) {
  setup(R"EOF(
name: dynamic-delay
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  static_delay: 1.33s
)EOF");
  getResponseFromUpstream();
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)->value().getStringView(), "1330");
  getResponseFromUpstream("{}");
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)->value().getStringView(), "1330");
  getResponseFromUpstream("{static_delay: \"0.2s\"}");
  // TODO(#392): This fails, because the duration is a two-field message: it would make here to see
  // both the number of seconds and nanoseconds to be overridden.
  // However, the seconds part is set to '0', which equates to the default of the underlying int
  // type, and the fact that we are using proto3, which doesn't merge default values.
  // Hence the following expectation will fail, as it yields 1200 instead of the expected 200.
  // EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)->value().getStringView(),
  // "200");
  getResponseFromUpstream("{static_delay: \"2.2s\"}");
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)->value().getStringView(), "2200");
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
  getResponseFromUpstream();
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)->value().getStringView(), "60");
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
    return Server::HttpDynamicDelayDecoderFilter::computeConcurrencyBasedLinearDelayMs(
        concurrency, minimal_delay, delay_factor);
  }
};

// Test that the delay looks as expected with various parameterizations.
TEST_F(ComputeTest, ComputeConcurrencyBasedLinearDelayMs) {
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
