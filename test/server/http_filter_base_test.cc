#include "server/http_dynamic_delay_filter.h"
#include "server/http_test_server_filter.h"
#include "server/http_time_tracking_filter.h"

#include "test/server/http_filter_integration_test_base.h"

#include "gtest/gtest.h"

namespace Nighthawk {

using namespace testing;

class HttpFilterBaseIntegrationTest
    : public HttpFilterIntegrationTestBase,
      public testing::TestWithParam<
          std::tuple<Envoy::Network::Address::IpVersion, absl::string_view, bool>> {
public:
  HttpFilterBaseIntegrationTest() : HttpFilterIntegrationTestBase(std::get<0>(GetParam())){};
};

INSTANTIATE_TEST_SUITE_P(
    IpVersions, HttpFilterBaseIntegrationTest,
    ::testing::Combine(testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
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
                       testing::ValuesIn({true, false})));

// Verify extensions are well-behaved when it comes to:
// - GET requests.
// - POST requests with an entity body (takes a slightly different code path).
// - Valid but empty configuration.
// - Bad request-level json configuration.
// We will be low on functional expectations for the extensions, but this will catch hangs and
// ensure that bad configuration handling is in-place.
TEST_P(HttpFilterBaseIntegrationTest, BasicExtensionFlows) {
  absl::string_view config = std::get<1>(GetParam());
  initializeConfig(std::string(config));
  bool is_post = std::get<2>(GetParam());
  if (is_post) {
    switchToPostWithEntityBody();
  }

  // Modulo the test-server, extensions are expected to need an upstream to synthesize a reply
  // when effective configuration is valid.
  ResponseOrigin happy_flow_response_origin = config.find_first_of("name: test-server") == 0
                                                  ? ResponseOrigin::EXTENSION
                                                  : ResponseOrigin::UPSTREAM;
  // Post without any request-level configuration. Should succeed.
  Envoy::IntegrationStreamDecoderPtr response = getResponse(happy_flow_response_origin);
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_TRUE(response->body().empty());

  // Test with a valid but empty request-level configuration.
  updateRequestLevelConfiguration("{}");
  response = getResponse(happy_flow_response_origin);
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_TRUE(response->body().empty());

  const std::string kBadConfigErrorSentinel =
      "didn't understand the request: Error merging json config: Unable to parse "
      "JSON as proto (INVALID_ARGUMENT:Unexpected";

  // When sending bad request-level configuration, the extension ought to reply directly.
  updateRequestLevelConfiguration("bad_json");
  response = getResponse(ResponseOrigin::EXTENSION);
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_THAT(response->body(), HasSubstr(kBadConfigErrorSentinel));

  // When sending empty request-level configuration, the extension ought to reply directly.
  updateRequestLevelConfiguration("");
  response = getResponse(ResponseOrigin::EXTENSION);
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_THAT(response->body(), HasSubstr(kBadConfigErrorSentinel));
}

} // namespace Nighthawk