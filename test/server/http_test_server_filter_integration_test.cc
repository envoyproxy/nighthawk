#include "api/server/response_options.pb.h"
#include "api/server/response_options.pb.validate.h"

#include "source/server/configuration.h"
#include "source/server/http_test_server_filter.h"

#include "test/server/http_filter_integration_test_base.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using namespace testing;

using ::testing::HasSubstr;

constexpr absl::string_view kDefaultProto = R"EOF(
name: test-server
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  response_body_size: 10
  response_headers:
  - { header: { key: "x-supplied-by", value: "nighthawk-test-server"} }
)EOF";

constexpr absl::string_view kNoConfigProto = R"EOF(
name: test-server
)EOF";

class HttpTestServerIntegrationTest : public HttpFilterIntegrationTestBase,
                                      public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  HttpTestServerIntegrationTest() : HttpFilterIntegrationTestBase(GetParam()) {}

  void testWithResponseSize(int response_body_size, bool expect_header = true) {
    setRequestLevelConfiguration(fmt::format("{{response_body_size:{}}}", response_body_size));
    Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
    ASSERT_TRUE(response->waitForEndStream());
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("200", response->headers().Status()->value().getStringView());
    if (expect_header) {
      auto inserted_header = response->headers().get(Envoy::Http::LowerCaseString("x-supplied-by"));
      ASSERT_EQ(1, inserted_header.size());
      EXPECT_EQ("nighthawk-test-server", inserted_header[0]->value().getStringView());
    }
    if (response_body_size == 0) {
      EXPECT_EQ(nullptr, response->headers().ContentType());
    } else {
      EXPECT_EQ("text/plain", response->headers().ContentType()->value().getStringView());
    }
    EXPECT_EQ(std::string(response_body_size, 'a'), response->body());
  }

  void testBadResponseSize(int response_body_size) {
    setRequestLevelConfiguration(fmt::format("{{response_body_size:{}}}", response_body_size));
    Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
    ASSERT_TRUE(response->waitForEndStream());
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("500", response->headers().Status()->value().getStringView());
  }
};

INSTANTIATE_TEST_SUITE_P(IpVersions, HttpTestServerIntegrationTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

TEST_P(HttpTestServerIntegrationTest, TestNoHeaderConfig) {
  initializeFilterConfiguration(kDefaultProto);
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_EQ(std::string(10, 'a'), response->body());
}

TEST_P(HttpTestServerIntegrationTest, TestBasics) {
  initializeFilterConfiguration(kDefaultProto);
  testWithResponseSize(1);
  testWithResponseSize(10);
  testWithResponseSize(100);
  testWithResponseSize(1000);
  testWithResponseSize(10000);
}

TEST_P(HttpTestServerIntegrationTest, TestNegative) {
  initializeFilterConfiguration(kDefaultProto);
  testBadResponseSize(-1);
}

// TODO(oschaaf): We can't currently override with a default value ('0') in this case.
TEST_P(HttpTestServerIntegrationTest, DISABLED_TestZeroLengthRequest) {
  initializeFilterConfiguration(kDefaultProto);
  testWithResponseSize(0);
}

TEST_P(HttpTestServerIntegrationTest, TestMaxBoundaryLengthRequest) {
  initializeFilterConfiguration(kDefaultProto);
  const int max = 1024 * 1024 * 4;
  testWithResponseSize(max);
}

TEST_P(HttpTestServerIntegrationTest, TestTooLarge) {
  initializeFilterConfiguration(kDefaultProto);
  const int max = 1024 * 1024 * 4;
  testBadResponseSize(max + 1);
}

TEST_P(HttpTestServerIntegrationTest, TestHeaderConfigUsingEnvoyApiV2) {
  initializeFilterConfiguration(kDefaultProto);
  setRequestLevelConfiguration(
      R"({response_headers: [ { header: { key: "foo", value: "bar2"}, append: true } ]})");
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  ASSERT_EQ(1, response->headers().get(Envoy::Http::LowerCaseString("foo")).size());
  EXPECT_EQ(
      "bar2",
      response->headers().get(Envoy::Http::LowerCaseString("foo"))[0]->value().getStringView());
  EXPECT_EQ(std::string(10, 'a'), response->body());
}

TEST_P(HttpTestServerIntegrationTest, TestHeaderConfigUsingEnvoyApiV3) {
  const std::string v3_configuration = R"EOF(
  name: test-server
  typed_config:
    "@type": type.googleapis.com/nighthawk.server.ResponseOptions
    response_body_size: 10
    v3_response_headers:
    - { header: { key: "foo", value: "bar2"}, append: true }
  )EOF";

  initializeFilterConfiguration(v3_configuration);
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  ASSERT_EQ(1, response->headers().get(Envoy::Http::LowerCaseString("foo")).size());
  EXPECT_EQ(
      "bar2",
      response->headers().get(Envoy::Http::LowerCaseString("foo"))[0]->value().getStringView());
  EXPECT_EQ(std::string(10, 'a'), response->body());
}

TEST_P(HttpTestServerIntegrationTest,
       DiesWhenRequestLevelConfigurationResultsInBothEnvoyApiV2AndV3ResponseHeadersSet) {
  initializeFilterConfiguration(kDefaultProto);
  setRequestLevelConfiguration(
      R"({v3_response_headers: [ { header: { key: "foo", value: "bar2"}, append: true } ]})");

  ASSERT_DEATH(ASSERT_TRUE(getResponse(ResponseOrigin::EXTENSION)->waitForEndStream()),
               HasSubstr("cannot specify both response_headers and v3_response_headers"));
}

TEST_P(HttpTestServerIntegrationTest,
       DiesWhenBothEnvoyApiV2AndV3ResponseHeadersAreSetInConfiguration) {
  const std::string invalid_configuration = R"EOF(
  name: test-server
  typed_config:
    "@type": type.googleapis.com/nighthawk.server.ResponseOptions
    response_headers:
      - { header: { key: "key1", value: "value1"} }
    v3_response_headers:
      - { header: { key: "key1", value: "value1"} }
  )EOF";

  ASSERT_DEATH(initializeFilterConfiguration(invalid_configuration),
               HasSubstr("cannot specify both response_headers and v3_response_headers"));
}

TEST_P(HttpTestServerIntegrationTest, TestEchoHeaders) {
  initializeFilterConfiguration(kDefaultProto);
  setRequestLevelConfiguration("{echo_request_headers: true}");
  setRequestHeader(Envoy::Http::LowerCaseString("gray"), "pidgeon");
  setRequestHeader(Envoy::Http::LowerCaseString("red"), "fox");
  setRequestHeader(Envoy::Http::LowerCaseString(":authority"), "foo.com");
  setRequestHeader(Envoy::Http::LowerCaseString(":path"), "/somepath");
  for (auto unique_header : {"one", "two", "three"}) {
    setRequestHeader(Envoy::Http::LowerCaseString("unique_header"), unique_header);
    Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
    ASSERT_TRUE(response->waitForEndStream());
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("200", response->headers().Status()->value().getStringView());
    EXPECT_THAT(response->body(), HasSubstr(R"(':authority', 'foo.com')"));
    EXPECT_THAT(response->body(), HasSubstr(R"(':path', '/somepath')"));
    EXPECT_THAT(response->body(), HasSubstr(R"(':method', 'GET')"));
    EXPECT_THAT(response->body(), HasSubstr(R"('gray', 'pidgeon')"));
    EXPECT_THAT(response->body(), HasSubstr(R"('red', 'fox')"));
    EXPECT_THAT(response->body(), HasSubstr(unique_header));
  }
}

TEST_P(HttpTestServerIntegrationTest, NoNoStaticConfigHeaderConfig) {
  initializeFilterConfiguration(kNoConfigProto);
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_EQ("", response->body());
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigBasics) {
  initializeFilterConfiguration(kNoConfigProto);
  testWithResponseSize(1, false);
  testWithResponseSize(10, false);
  testWithResponseSize(100, false);
  testWithResponseSize(1000, false);
  testWithResponseSize(10000, false);
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigNegative) {
  initializeFilterConfiguration(kNoConfigProto);
  testBadResponseSize(-1);
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigZeroLengthRequest) {
  initializeFilterConfiguration(kNoConfigProto);
  testWithResponseSize(0, false);
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigMaxBoundaryLengthRequest) {
  initializeFilterConfiguration(kNoConfigProto);
  const int max = 1024 * 1024 * 4;
  testWithResponseSize(max, false);
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigTooLarge) {
  initializeFilterConfiguration(kNoConfigProto);
  const int max = 1024 * 1024 * 4;
  testBadResponseSize(max + 1);
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigHeaderConfig) {
  initializeFilterConfiguration(kNoConfigProto);
  setRequestLevelConfiguration(
      R"({response_headers: [ { header: { key: "foo", value: "bar2"}, append: true } ]})");
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);

  ASSERT_TRUE(response->waitForEndStream());
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  ASSERT_EQ(1, response->headers().get(Envoy::Http::LowerCaseString("foo")).size());
  EXPECT_EQ(
      "bar2",
      response->headers().get(Envoy::Http::LowerCaseString("foo"))[0]->value().getStringView());
  EXPECT_EQ("", response->body());
}

} // namespace
} // namespace Nighthawk
