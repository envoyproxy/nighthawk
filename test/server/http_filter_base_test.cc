#include "server/http_dynamic_delay_filter.h"
#include "server/http_test_server_filter.h"
#include "server/http_time_tracking_filter.h"

#include "test/server/http_filter_integration_test_base.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::testing::HasSubstr;

enum TestRequestMethod { GET, POST };

const std::string kBadConfigErrorSentinel =
    "didn't understand the request: Error merging json config: Unable to parse "
    "JSON as proto (INVALID_ARGUMENT:Unexpected";

class HttpFilterBaseIntegrationTest
    : public HttpFilterIntegrationTestBase,
      public testing::TestWithParam<
          std::tuple<Envoy::Network::Address::IpVersion, absl::string_view, TestRequestMethod>> {
public:
  HttpFilterBaseIntegrationTest()
      : HttpFilterIntegrationTestBase(std::get<0>(GetParam())), config_(std::get<1>(GetParam())) {
    initializeFilterConfiguration(config_);
    if (std::get<2>(GetParam()) == TestRequestMethod::POST) {
      switchToPostWithEntityBody();
    }
  };

  ResponseOrigin getHappyFlowResponseOrigin() {
    // Modulo the test-server, extensions are expected to need an upstream to synthesize a reply
    // when the effective configuration is valid.
    return config_.find_first_of("name: test-server") == 0 ? ResponseOrigin::EXTENSION
                                                           : ResponseOrigin::UPSTREAM;
  }

protected:
  const std::string config_;
};

INSTANTIATE_TEST_SUITE_P(
    IpVersions, HttpFilterBaseIntegrationTest,
    testing::Combine(testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                     testing::ValuesIn({absl::string_view(R"EOF(
name: time-tracking
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  emit_previous_request_delta_in_response_header: "foo"
)EOF"),
                                        absl::string_view(R"EOF(
name: dynamic-delay
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  static_delay: 0.1s
)EOF"),
                                        absl::string_view("name: test-server")}),
                     testing::ValuesIn({TestRequestMethod::GET, TestRequestMethod::POST})));

TEST_P(HttpFilterBaseIntegrationTest, NoRequestLevelConfigurationShouldSucceed) {
  Envoy::IntegrationStreamDecoderPtr response = getResponse(getHappyFlowResponseOrigin());
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_TRUE(response->body().empty());
}

TEST_P(HttpFilterBaseIntegrationTest, EmptyJsonRequestLevelConfigurationShouldSucceed) {
  setRequestLevelConfiguration("{}");
  Envoy::IntegrationStreamDecoderPtr response = getResponse(getHappyFlowResponseOrigin());
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_TRUE(response->body().empty());
}

TEST_P(HttpFilterBaseIntegrationTest, BadJsonAsRequestLevelConfigurationShouldFail) {
  // When sending bad request-level configuration, the extension ought to reply directly.
  setRequestLevelConfiguration("bad_json");
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_THAT(response->body(), HasSubstr(kBadConfigErrorSentinel));
}

TEST_P(HttpFilterBaseIntegrationTest, EmptyRequestLevelConfigurationShouldFail) {
  // When sending empty request-level configuration, the extension ought to reply directly.
  setRequestLevelConfiguration("");
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  ASSERT_TRUE(response->waitForEndStream());
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_THAT(response->body(), HasSubstr(kBadConfigErrorSentinel));
}

TEST_P(HttpFilterBaseIntegrationTest, MultipleValidConfigurationHeadersFails) {
  // Make sure we fail when two valid configuration headers are send.
  setRequestLevelConfiguration("{}");
  appendRequestLevelConfiguration("{}");
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_THAT(response->body(),
              HasSubstr("Received multiple configuration headers in the request"));
}

TEST_P(HttpFilterBaseIntegrationTest, SingleValidPlusEmptyConfigurationHeadersFails) {
  // Make sure we fail when both a valid configuration plus an empty configuration header is send.
  setRequestLevelConfiguration("{}");
  appendRequestLevelConfiguration("");
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_THAT(response->body(),
              HasSubstr("Received multiple configuration headers in the request"));
}

} // namespace
} // namespace Nighthawk
