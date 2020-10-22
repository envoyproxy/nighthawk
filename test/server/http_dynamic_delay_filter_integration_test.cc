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
 *
 * The Dynamic Delay filter communicates with the fault filter by adding kDelayHeaderString
 * to the request headers. We use that in tests below to verify expectations. The fault filter
 * accepts input values via request headers specified in milliseconds, so our expectations are
 * also using milliseconds.
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
  initializeFilterConfiguration(R"(
name: dynamic-delay
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
)");
  // Don't send any config request header ...
  getResponse(ResponseOrigin::UPSTREAM);
  // ... we shouldn't observe any delay being requested via the upstream request headers.
  EXPECT_TRUE(upstream_request_->headers().get(kDelayHeaderString).empty());

  // Send a config request header with an empty / default configuration ....
  setRequestLevelConfiguration("{}");
  getResponse(ResponseOrigin::UPSTREAM);
  // ... we shouldn't observe any delay being requested via the upstream request headers.
  EXPECT_TRUE(upstream_request_->headers().get(kDelayHeaderString).empty());

  // Send a config request header requesting a 1.6s delay...
  setRequestLevelConfiguration("{static_delay: \"1.6s\"}");
  getResponse(ResponseOrigin::UPSTREAM);
  // ...we should observe a delay of 1.6s in the upstream request.
  ASSERT_EQ(upstream_request_->headers().get(kDelayHeaderString).size(), 1);
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)[0]->value().getStringView(),
            "1600");
}

// Verify expectations with static/file-based static_delay configuration.
TEST_P(HttpDynamicDelayIntegrationTest, StaticConfigurationStaticDelay) {
  initializeFilterConfiguration(R"EOF(
name: dynamic-delay
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  static_delay: 1.33s
)EOF");

  // Without any request-level configuration, we expect the statically configured static delay to
  // apply.
  getResponse(ResponseOrigin::UPSTREAM);
  ASSERT_EQ(upstream_request_->headers().get(kDelayHeaderString).size(), 1);
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)[0]->value().getStringView(),
            "1330");

  // With an empty request-level configuration, we expect the statically configured static delay to
  // apply.
  setRequestLevelConfiguration("{}");
  getResponse(ResponseOrigin::UPSTREAM);
  ASSERT_EQ(upstream_request_->headers().get(kDelayHeaderString).size(), 1);
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)[0]->value().getStringView(),
            "1330");

  // Overriding the statically configured static delay via request-level configuration should be
  // reflected in the output.
  setRequestLevelConfiguration("{static_delay: \"0.2s\"}");
  getResponse(ResponseOrigin::UPSTREAM);
  // TODO(#392): This fails, because the duration is a two-field message: it would make here to see
  // both the number of seconds and nanoseconds to be overridden.
  // However, the seconds part is set to '0', which equates to the default of the underlying int
  // type, and the fact that we are using proto3, which doesn't merge default values.
  // Hence the following expectation will fail, as it yields 1200 instead of the expected 200.
  // EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)[0]->value().getStringView(),
  // "200");

  // Overriding the statically configured static delay via request-level configuration should be
  // reflected in the output.
  setRequestLevelConfiguration("{static_delay: \"2.2s\"}");
  getResponse(ResponseOrigin::UPSTREAM);
  // 2.2 seconds -> 2200 ms.
  ASSERT_EQ(upstream_request_->headers().get(kDelayHeaderString).size(), 1);
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)[0]->value().getStringView(),
            "2200");
}

// Verify expectations with static/file-based concurrency_based_linear_delay configuration.
TEST_P(HttpDynamicDelayIntegrationTest, StaticConfigurationConcurrentDelay) {
  initializeFilterConfiguration(R"EOF(
name: dynamic-delay
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  concurrency_based_linear_delay:
    minimal_delay: 0.05s
    concurrency_delay_factor: 0.01s
)EOF");
  getResponse(ResponseOrigin::UPSTREAM);
  // Based on the algorithm of concurrency_based_linear_delay, for the first request we expect to
  // observe the configured minimal_delay + concurrency_delay_factor = 0.06s -> 60ms.
  ASSERT_EQ(upstream_request_->headers().get(kDelayHeaderString).size(), 1);
  EXPECT_EQ(upstream_request_->headers().get(kDelayHeaderString)[0]->value().getStringView(), "60");
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
