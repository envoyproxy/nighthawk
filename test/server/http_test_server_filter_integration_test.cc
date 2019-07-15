#include "envoy/upstream/cluster_manager.h"
#include "envoy/upstream/upstream.h"

#include "common/api/api_impl.h"

#include "server/http_test_server_filter.h"

#include "test/common/upstream/utility.h"
#include "test/integration/http_integration.h"

#include "api/server/response_options.pb.h"
#include "api/server/response_options.pb.validate.h"
#include "gtest/gtest.h"

namespace Nighthawk {

using namespace testing;

class HttpTestServerIntegrationTestBase : public Envoy::HttpIntegrationTest,
                                          public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  HttpTestServerIntegrationTestBase()
      : HttpIntegrationTest(Envoy::Http::CodecClient::Type::HTTP1, GetParam(), realTime()) {}

  // TODO(oschaaf): Modify Envoy's Envoy::IntegrationUtil::makeSingleRequest() to allow for a way to
  // manipulate the request headers before they get send. Then we can eliminate these copies.
  Envoy::BufferingStreamDecoderPtr makeSingleRequest(
      uint32_t port, absl::string_view method, absl::string_view url, absl::string_view body,
      Envoy::Http::CodecClient::Type type, Envoy::Network::Address::IpVersion ip_version,
      absl::string_view host, absl::string_view content_type,
      const std::function<void(Envoy::Http::HeaderMapImpl&)>& request_header_delegate) {
    auto addr = Envoy::Network::Utility::resolveUrl(fmt::format(
        "tcp://{}:{}", Envoy::Network::Test::getLoopbackAddressUrlString(ip_version), port));
    return makeSingleRequest(addr, method, url, body, type, host, content_type,
                             request_header_delegate);
  }

  Envoy::BufferingStreamDecoderPtr makeSingleRequest(
      const Envoy::Network::Address::InstanceConstSharedPtr& addr, absl::string_view method,
      absl::string_view url, absl::string_view body, Envoy::Http::CodecClient::Type type,
      absl::string_view host, absl::string_view content_type,
      const std::function<void(Envoy::Http::HeaderMapImpl&)>& request_header_delegate) {

    NiceMock<Envoy::Stats::MockIsolatedStatsStore> mock_stats_store;
    Envoy::Event::GlobalTimeSystem time_system;
    Envoy::Api::Impl api(Envoy::Thread::threadFactoryForTest(), mock_stats_store, time_system,
                         Envoy::Filesystem::fileSystemForTest());
    Envoy::Event::DispatcherPtr dispatcher(api.allocateDispatcher());
    std::shared_ptr<Envoy::Upstream::MockClusterInfo> cluster{
        new NiceMock<Envoy::Upstream::MockClusterInfo>()};
    Envoy::Upstream::HostDescriptionConstSharedPtr host_description{
        Envoy::Upstream::makeTestHostDescription(cluster, "tcp://127.0.0.1:80")};
    Envoy::Http::CodecClientProd client(
        type,
        dispatcher->createClientConnection(addr, Envoy::Network::Address::InstanceConstSharedPtr(),
                                           Envoy::Network::Test::createRawBufferSocket(), nullptr),
        host_description, *dispatcher);
    Envoy::BufferingStreamDecoderPtr response(
        new Envoy::BufferingStreamDecoder([&client, &dispatcher]() -> void {
          client.close();
          dispatcher->exit();
        }));
    Envoy::Http::StreamEncoder& encoder = client.newStream(*response);
    encoder.getStream().addCallbacks(*response);

    Envoy::Http::HeaderMapImpl headers;
    headers.insertMethod().value(method);
    headers.insertPath().value(url);
    headers.insertHost().value(host);
    headers.insertScheme().value(Envoy::Http::Headers::get().SchemeValues.Http);
    if (!content_type.empty()) {
      headers.insertContentType().value(content_type);
    }
    request_header_delegate(headers);
    encoder.encodeHeaders(headers, body.empty());
    if (!body.empty()) {
      Envoy::Buffer::OwnedImpl body_buffer(body);
      encoder.encodeData(body_buffer, true);
    }

    dispatcher->run(Envoy::Event::Dispatcher::RunType::Block);
    return response;
  }

  void testWithResponseSize(int response_body_size, bool expect_header = true) {
    Envoy::BufferingStreamDecoderPtr response = makeSingleRequest(
        lookupPort("http"), "GET", "/", "", downstream_protocol_, version_, "foo.com", "",
        [response_body_size](Envoy::Http::HeaderMapImpl& request_headers) {
          const std::string header_config =
              fmt::format("{{response_body_size:{}}}", response_body_size);
          request_headers.addCopy(
              Nighthawk::Server::TestServer::HeaderNames::get().TestServerConfig, header_config);
        });
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
    Envoy::BufferingStreamDecoderPtr response = makeSingleRequest(
        lookupPort("http"), "GET", "/", "", downstream_protocol_, version_, "foo.com", "",
        [response_body_size](Envoy::Http::HeaderMapImpl& request_headers) {
          const std::string header_config =
              fmt::format("{{response_body_size:{}}}", response_body_size);
          request_headers.addCopy(
              Nighthawk::Server::TestServer::HeaderNames::get().TestServerConfig, header_config);
        });
    ASSERT_TRUE(response->complete());
    EXPECT_EQ("500", response->headers().Status()->value().getStringView());
  }
};

class HttpTestServerIntegrationTest : public HttpTestServerIntegrationTestBase {
public:
  void SetUp() override { initialize(); }

  void initialize() override {
    config_helper_.addFilter(R"EOF(
name: test-server
config:
  response_body_size: 10
  response_headers:
  - { header: { key: "x-supplied-by", value: "nighthawk-test-server"} }
)EOF");
    HttpTestServerIntegrationTestBase::initialize();
  }

  void TearDown() override {
    cleanupUpstreamAndDownstream();
    test_server_.reset();
    fake_upstreams_.clear();
  }
};

INSTANTIATE_TEST_SUITE_P(IpVersions, HttpTestServerIntegrationTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

TEST_P(HttpTestServerIntegrationTest, TestNoHeaderConfig) {
  Envoy::BufferingStreamDecoderPtr response =
      makeSingleRequest(lookupPort("http"), "GET", "/", "", downstream_protocol_, version_,
                        "foo.com", "", [](Envoy::Http::HeaderMapImpl&) {});
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_EQ(std::string(10, 'a'), response->body());
}

TEST_P(HttpTestServerIntegrationTest, TestBasics) {
  testWithResponseSize(1);
  testWithResponseSize(10);
  testWithResponseSize(100);
  testWithResponseSize(1000);
  testWithResponseSize(10000);
}

TEST_P(HttpTestServerIntegrationTest, TestNegative) { testBadResponseSize(-1); }

// TODO(oschaaf): We can't currently override with a default value ('0') in this case.
TEST_P(HttpTestServerIntegrationTest, DISABLED_TestZeroLengthRequest) { testWithResponseSize(0); }

TEST_P(HttpTestServerIntegrationTest, TestMaxBoundaryLengthRequest) {
  const int max = 1024 * 1024 * 4;
  testWithResponseSize(max);
}

TEST_P(HttpTestServerIntegrationTest, TestTooLarge) {
  const int max = 1024 * 1024 * 4;
  testBadResponseSize(max + 1);
}

TEST_P(HttpTestServerIntegrationTest, TestHeaderConfig) {
  Envoy::BufferingStreamDecoderPtr response = makeSingleRequest(
      lookupPort("http"), "GET", "/", "", downstream_protocol_, version_, "foo.com", "",
      [](Envoy::Http::HeaderMapImpl& request_headers) {
        const std::string header_config =
            R"({response_headers: [ { header: { key: "foo", value: "bar2"}, append: true } ]})";
        request_headers.addCopy(Nighthawk::Server::TestServer::HeaderNames::get().TestServerConfig,
                                header_config);
      });
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_EQ("bar2",
            response->headers().get(Envoy::Http::LowerCaseString("foo"))->value().getStringView());
  EXPECT_EQ(std::string(10, 'a'), response->body());
}

class HttpTestServerIntegrationNoConfigTest : public HttpTestServerIntegrationTestBase {
public:
  void SetUp() override { initialize(); }

  void TearDown() override {
    cleanupUpstreamAndDownstream();
    test_server_.reset();
    fake_upstreams_.clear();
  }

  void initialize() override {
    config_helper_.addFilter(R"EOF(
name: test-server
)EOF");
    HttpTestServerIntegrationTestBase::initialize();
  }
};

INSTANTIATE_TEST_SUITE_P(IpVersions, HttpTestServerIntegrationNoConfigTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

TEST_P(HttpTestServerIntegrationNoConfigTest, TestNoHeaderConfig) {
  Envoy::BufferingStreamDecoderPtr response =
      makeSingleRequest(lookupPort("http"), "GET", "/", "", downstream_protocol_, version_,
                        "foo.com", "", [](Envoy::Http::HeaderMapImpl&) {});
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_EQ("", response->body());
}

TEST_P(HttpTestServerIntegrationNoConfigTest, TestBasics) {
  testWithResponseSize(1, false);
  testWithResponseSize(10, false);
  testWithResponseSize(100, false);
  testWithResponseSize(1000, false);
  testWithResponseSize(10000, false);
}

TEST_P(HttpTestServerIntegrationNoConfigTest, TestNegative) { testBadResponseSize(-1); }

TEST_P(HttpTestServerIntegrationNoConfigTest, TestZeroLengthRequest) {
  testWithResponseSize(0, false);
}

TEST_P(HttpTestServerIntegrationNoConfigTest, TestMaxBoundaryLengthRequest) {
  const int max = 1024 * 1024 * 4;
  testWithResponseSize(max, false);
}

TEST_P(HttpTestServerIntegrationNoConfigTest, TestTooLarge) {
  const int max = 1024 * 1024 * 4;
  testBadResponseSize(max + 1);
}

TEST_P(HttpTestServerIntegrationNoConfigTest, TestHeaderConfig) {
  Envoy::BufferingStreamDecoderPtr response = makeSingleRequest(
      lookupPort("http"), "GET", "/", "", downstream_protocol_, version_, "foo.com", "",
      [](Envoy::Http::HeaderMapImpl& request_headers) {
        const std::string header_config =
            R"({response_headers: [ { header: { key: "foo", value: "bar2"}, append: true } ]})";
        request_headers.addCopy(Nighthawk::Server::TestServer::HeaderNames::get().TestServerConfig,
                                header_config);
      });
  ASSERT_TRUE(response->complete());
  EXPECT_EQ("200", response->headers().Status()->value().getStringView());
  EXPECT_EQ("bar2",
            response->headers().get(Envoy::Http::LowerCaseString("foo"))->value().getStringView());
  EXPECT_EQ("", response->body());
}

class HttpTestServerDecoderFilterTest : public Test {};

// Here we test config-level merging as well as its application at the response-header level.
TEST_F(HttpTestServerDecoderFilterTest, HeaderMerge) {
  nighthawk::server::ResponseOptions initial_options;
  auto response_header = initial_options.add_response_headers();
  response_header->mutable_header()->set_key("foo");
  response_header->mutable_header()->set_value("bar1");
  response_header->mutable_append();

  Server::HttpTestServerDecoderFilterConfigSharedPtr config =
      std::make_shared<Server::HttpTestServerDecoderFilterConfig>(initial_options);
  Server::HttpTestServerDecoderFilter f(config);
  absl::optional<std::string> error_message;
  nighthawk::server::ResponseOptions options = config->server_config();

  EXPECT_EQ(1, options.response_headers_size());

  EXPECT_EQ("foo", options.response_headers(0).header().key());
  EXPECT_EQ("bar1", options.response_headers(0).header().value());
  EXPECT_EQ(false, options.response_headers(0).append().value());

  Envoy::Http::TestHeaderMapImpl header_map{{":status", "200"}, {"foo", "bar"}};
  f.applyConfigToResponseHeaders(header_map, options);
  EXPECT_TRUE(Envoy::TestUtility::headerMapEqualIgnoreOrder(
      header_map, Envoy::Http::TestHeaderMapImpl{{":status", "200"}, {"foo", "bar1"}}));

  EXPECT_TRUE(f.mergeJsonConfig(
      R"({response_headers: [ { header: { key: "foo", value: "bar2"}, append: false } ]})", options,
      error_message));
  EXPECT_EQ(absl::nullopt, error_message);
  EXPECT_EQ(2, options.response_headers_size());

  EXPECT_EQ("foo", options.response_headers(1).header().key());
  EXPECT_EQ("bar2", options.response_headers(1).header().value());
  EXPECT_EQ(false, options.response_headers(1).append().value());

  f.applyConfigToResponseHeaders(header_map, options);
  EXPECT_TRUE(Envoy::TestUtility::headerMapEqualIgnoreOrder(
      header_map, Envoy::Http::TestHeaderMapImpl{{":status", "200"}, {"foo", "bar2"}}));

  EXPECT_TRUE(f.mergeJsonConfig(
      R"({response_headers: [ { header: { key: "foo2", value: "bar3"}, append: true } ]})", options,
      error_message));
  EXPECT_EQ(absl::nullopt, error_message);
  EXPECT_EQ(3, options.response_headers_size());

  EXPECT_EQ("foo2", options.response_headers(2).header().key());
  EXPECT_EQ("bar3", options.response_headers(2).header().value());
  EXPECT_EQ(true, options.response_headers(2).append().value());

  f.applyConfigToResponseHeaders(header_map, options);
  EXPECT_TRUE(Envoy::TestUtility::headerMapEqualIgnoreOrder(
      header_map,
      Envoy::Http::TestHeaderMapImpl{{":status", "200"}, {"foo", "bar2"}, {"foo2", "bar3"}}));

  EXPECT_FALSE(f.mergeJsonConfig(R"(bad_json)", options, error_message));
  EXPECT_EQ("Error merging json config: Unable to parse JSON as proto (INVALID_ARGUMENT:Unexpected "
            "token.\nbad_json\n^): bad_json",
            error_message);
  EXPECT_EQ(3, options.response_headers_size());
}

} // namespace Nighthawk
