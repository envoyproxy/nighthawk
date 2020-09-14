#include "api/server/response_options.pb.h"
#include "api/server/response_options.pb.validate.h"

#include "server/configuration.h"
#include "server/http_test_server_filter.h"

#include "test/server/http_filter_integration_test_base.h"

#include "gtest/gtest.h"

namespace Nighthawk {

using namespace testing;

const std::string kDefaultProto = R"EOF(
name: test-server
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  response_body_size: 10
  response_headers:
  - { header: { key: "x-supplied-by", value: "nighthawk-test-server"} }
)EOF";

const std::string kNoConfigProto = R"EOF(
name: test-server
)EOF";

class HttpTestServerIntegrationTest : public HttpFilterIntegrationTestBase,
                                      public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  HttpTestServerIntegrationTest() : HttpFilterIntegrationTestBase(GetParam()) {}

  void testWithResponseSize(int response_body_size, bool expect_header = true) {
    updateRequestLevelConfiguration(fmt::format("{{response_body_size:{}}}", response_body_size));
    Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("200", response->headers().Status()->value().getStringView());
    if (expect_header) {
      auto inserted_header = response->headers().get(Envoy::Http::LowerCaseString("x-supplied-by"));
      ASSERT_NE(nullptr, inserted_header);
      EXPECT_EQ("nighthawk-test-server", inserted_header->value().getStringView());
    }
    if (response_body_size == 0) {
      EXPECT_EQ(nullptr, response->headers().ContentType());
    } else {
      EXPECT_EQ("text/plain", response->headers().ContentType()->value().getStringView());
    }
    EXPECT_EQ(std::string(response_body_size, 'a'), response->body());
  }

  void testBadResponseSize(int response_body_size) {
    updateRequestLevelConfiguration(fmt::format("{{response_body_size:{}}}", response_body_size));
    Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("500", response->headers().Status()->value().getStringView());
  }
};

INSTANTIATE_TEST_SUITE_P(IpVersions, HttpTestServerIntegrationTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

TEST_P(HttpTestServerIntegrationTest, TestNoHeaderConfig) {
  setup(kDefaultProto);
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_EQ(std::string(10, 'a'), response->body());
}

TEST_P(HttpTestServerIntegrationTest, TestBasics) {
  setup(kDefaultProto);
  testWithResponseSize(1);
  testWithResponseSize(10);
  testWithResponseSize(100);
  testWithResponseSize(1000);
  testWithResponseSize(10000);
}

TEST_P(HttpTestServerIntegrationTest, TestNegative) {
  setup(kDefaultProto);
  testBadResponseSize(-1);
}

// TODO(oschaaf): We can't currently override with a default value ('0') in this case.
TEST_P(HttpTestServerIntegrationTest, DISABLED_TestZeroLengthRequest) {
  setup(kDefaultProto);
  testWithResponseSize(0);
}

TEST_P(HttpTestServerIntegrationTest, TestMaxBoundaryLengthRequest) {
  setup(kDefaultProto);
  const int max = 1024 * 1024 * 4;
  testWithResponseSize(max);
}

TEST_P(HttpTestServerIntegrationTest, TestTooLarge) {
  setup(kDefaultProto);
  const int max = 1024 * 1024 * 4;
  testBadResponseSize(max + 1);
}

TEST_P(HttpTestServerIntegrationTest, TestHeaderConfig) {
  setup(kDefaultProto);
  updateRequestLevelConfiguration(
      R"({response_headers: [ { header: { key: "foo", value: "bar2"}, append: true } ]})");
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_EQ("bar2",
            response->headers().get(Envoy::Http::LowerCaseString("foo"))->value().getStringView());
  EXPECT_EQ(std::string(10, 'a'), response->body());
}

TEST_P(HttpTestServerIntegrationTest, TestEchoHeaders) {
  setup(kDefaultProto);
  updateRequestLevelConfiguration("{echo_request_headers: true}");
  setRequestHeader(Envoy::Http::LowerCaseString("gray"), "pidgeon");
  setRequestHeader(Envoy::Http::LowerCaseString("red"), "fox");
  setRequestHeader(Envoy::Http::LowerCaseString(":authority"), "foo.com");
  setRequestHeader(Envoy::Http::LowerCaseString(":path"), "/somepath");
  for (auto unique_header : {"one", "two", "three"}) {
    setRequestHeader(Envoy::Http::LowerCaseString("unique_header"), unique_header);
    Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
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
  setup(kNoConfigProto);
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_EQ("", response->body());
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigBasics) {
  setup(kNoConfigProto);
  testWithResponseSize(1, false);
  testWithResponseSize(10, false);
  testWithResponseSize(100, false);
  testWithResponseSize(1000, false);
  testWithResponseSize(10000, false);
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigNegative) {
  setup(kNoConfigProto);
  testBadResponseSize(-1);
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigZeroLengthRequest) {
  setup(kNoConfigProto);
  testWithResponseSize(0, false);
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigMaxBoundaryLengthRequest) {
  setup(kNoConfigProto);
  const int max = 1024 * 1024 * 4;
  testWithResponseSize(max, false);
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigTooLarge) {
  setup(kNoConfigProto);
  const int max = 1024 * 1024 * 4;
  testBadResponseSize(max + 1);
}

TEST_P(HttpTestServerIntegrationTest, TestNoStaticConfigHeaderConfig) {
  setup(kNoConfigProto);
  updateRequestLevelConfiguration(
      R"({response_headers: [ { header: { key: "foo", value: "bar2"}, append: true } ]})");
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);

  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_EQ("bar2",
            response->headers().get(Envoy::Http::LowerCaseString("foo"))->value().getStringView());
  EXPECT_EQ("", response->body());
}

// Here we test config-level merging as well as its application at the response-header level.
TEST(HttpTestServerDecoderFilterTest, HeaderMerge) {
  nighthawk::server::ResponseOptions initial_options;
  auto response_header = initial_options.add_response_headers();
  response_header->mutable_header()->set_key("foo");
  response_header->mutable_header()->set_value("bar1");
  response_header->mutable_append();

  Server::HttpTestServerDecoderFilterConfigSharedPtr config =
      std::make_shared<Server::HttpTestServerDecoderFilterConfig>(initial_options);
  Server::HttpTestServerDecoderFilter f(config);

  absl::StatusOr<Server::EffectiveFilterConfigurationPtr> options_or =
      config->getEffectiveConfiguration();
  ASSERT_TRUE(options_or.ok());
  nighthawk::server::ResponseOptions options = *options_or.value();
  EXPECT_EQ(1, options.response_headers_size());

  EXPECT_EQ("foo", options.response_headers(0).header().key());
  EXPECT_EQ("bar1", options.response_headers(0).header().value());
  EXPECT_EQ(false, options.response_headers(0).append().value());

  Envoy::Http::TestResponseHeaderMapImpl header_map{{":status", "200"}, {"foo", "bar"}};
  Server::Configuration::applyConfigToResponseHeaders(header_map, options);
  EXPECT_TRUE(Envoy::TestUtility::headerMapEqualIgnoreOrder(
      header_map, Envoy::Http::TestResponseHeaderMapImpl{{":status", "200"}, {"foo", "bar1"}}));

  std::string error_message;
  EXPECT_TRUE(Server::Configuration::mergeJsonConfig(
      R"({response_headers: [ { header: { key: "foo", value: "bar2"}, append: false } ]})", options,
      error_message));
  EXPECT_EQ("", error_message);
  EXPECT_EQ(2, options.response_headers_size());

  EXPECT_EQ("foo", options.response_headers(1).header().key());
  EXPECT_EQ("bar2", options.response_headers(1).header().value());
  EXPECT_EQ(false, options.response_headers(1).append().value());

  Server::Configuration::applyConfigToResponseHeaders(header_map, options);
  EXPECT_TRUE(Envoy::TestUtility::headerMapEqualIgnoreOrder(
      header_map, Envoy::Http::TestRequestHeaderMapImpl{{":status", "200"}, {"foo", "bar2"}}));

  EXPECT_TRUE(Server::Configuration::mergeJsonConfig(
      R"({response_headers: [ { header: { key: "foo2", value: "bar3"}, append: true } ]})", options,
      error_message));
  EXPECT_EQ("", error_message);
  EXPECT_EQ(3, options.response_headers_size());

  EXPECT_EQ("foo2", options.response_headers(2).header().key());
  EXPECT_EQ("bar3", options.response_headers(2).header().value());
  EXPECT_EQ(true, options.response_headers(2).append().value());

  Server::Configuration::applyConfigToResponseHeaders(header_map, options);
  EXPECT_TRUE(Envoy::TestUtility::headerMapEqualIgnoreOrder(
      header_map, Envoy::Http::TestResponseHeaderMapImpl{
                      {":status", "200"}, {"foo", "bar2"}, {"foo2", "bar3"}}));

  EXPECT_FALSE(Server::Configuration::mergeJsonConfig("bad_json", options, error_message));
  EXPECT_EQ("Error merging json config: Unable to parse JSON as proto (INVALID_ARGUMENT:Unexpected "
            "token.\nbad_json\n^): bad_json",
            error_message);
  EXPECT_EQ(3, options.response_headers_size());
}

} // namespace Nighthawk
