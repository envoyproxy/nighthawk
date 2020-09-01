#include <chrono>

#include "envoy/upstream/cluster_manager.h"
#include "envoy/upstream/upstream.h"

#include "external/envoy/test/common/upstream/utility.h"
#include "external/envoy/test/integration/http_integration.h"
#include "external/envoy/test/test_common/simulated_time_system.h"

#include "api/server/response_options.pb.h"
#include "api/server/response_options.pb.validate.h"

#include "server/configuration.h"
#include "server/http_time_tracking_filter.h"
#include "server/well_known_headers.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using namespace std::chrono_literals;

const std::string kLatencyResponseHeaderName = "x-prd";
const std::string kDefaultProtoFragment = fmt::format(
    "emit_previous_request_delta_in_response_header: \"{}\"", kLatencyResponseHeaderName);
const std::string kProtoConfigTemplate = R"EOF(
name: time-tracking
typed_config:
  "@type": type.googleapis.com/nighthawk.server.ResponseOptions
  {}
)EOF";

class HttpTimeTrackingIntegrationTest
    : public Envoy::HttpIntegrationTest,
      public testing::TestWithParam<Envoy::Network::Address::IpVersion> {
protected:
  HttpTimeTrackingIntegrationTest()
      : HttpIntegrationTest(Envoy::Http::CodecClient::Type::HTTP1, GetParam()),
        request_headers_({{":method", "GET"}, {":path", "/"}, {":authority", "host"}}) {}

  // We don't override SetUp(): tests in this file will call setup() instead to avoid having to
  // create a fixture per filter configuration.
  void setup(const std::string& config) {
    config_helper_.addFilter(config);
    HttpIntegrationTest::initialize();
  }

  // Fetches a response with request-level configuration set in the request header.
  Envoy::IntegrationStreamDecoderPtr getResponse(absl::string_view request_level_config,
                                                 bool setup_for_upstream_request = true) {
    Envoy::Http::TestRequestHeaderMapImpl request_headers = request_headers_;
    request_headers.setCopy(Nighthawk::Server::TestServer::HeaderNames::get().TestServerConfig,
                            request_level_config);
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
};

INSTANTIATE_TEST_SUITE_P(IpVersions, HttpTimeTrackingIntegrationTest,
                         testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

// Verify expectations with static/file-based time-tracking configuration.
TEST_P(HttpTimeTrackingIntegrationTest, ReturnsPositiveLatencyForStaticConfiguration) {
  setup(fmt::format(kProtoConfigTemplate, kDefaultProtoFragment));
  Envoy::IntegrationStreamDecoderPtr response = getResponse();
  int64_t latency;
  const Envoy::Http::HeaderEntry* latency_header_1 =
      response->headers().get(Envoy::Http::LowerCaseString(kLatencyResponseHeaderName));
  EXPECT_EQ(latency_header_1, nullptr);
  response = getResponse();
  const Envoy::Http::HeaderEntry* latency_header_2 =
      response->headers().get(Envoy::Http::LowerCaseString(kLatencyResponseHeaderName));
  ASSERT_NE(latency_header_2, nullptr);
  EXPECT_TRUE(absl::SimpleAtoi(latency_header_2->value().getStringView(), &latency));
  EXPECT_GT(latency, 0);
}

// Verify expectations with an empty time-tracking configuration.
TEST_P(HttpTimeTrackingIntegrationTest, ReturnsPositiveLatencyForPerRequestConfiguration) {
  setup(fmt::format(kProtoConfigTemplate, ""));
  // Don't send any config request header
  getResponse();
  // Send a config request header with an empty / default config. Should be a no-op.
  getResponse("{}");
  // Send a config request header, this should become effective.
  Envoy::IntegrationStreamDecoderPtr response =
      getResponse(fmt::format("{{{}}}", kDefaultProtoFragment));
  const Envoy::Http::HeaderEntry* latency_header =
      response->headers().get(Envoy::Http::LowerCaseString(kLatencyResponseHeaderName));
  ASSERT_NE(latency_header, nullptr);
  int64_t latency;
  EXPECT_TRUE(absl::SimpleAtoi(latency_header->value().getStringView(), &latency));
  // TODO(oschaaf): figure out if we can use simtime here, and verify actual timing matches
  // what we'd expect using that.
  EXPECT_GT(latency, 0);
}

TEST_P(HttpTimeTrackingIntegrationTest, BehavesWellWithBadPerRequestConfiguration) {
  setup(fmt::format(kProtoConfigTemplate, ""));
  // Send a malformed config request header. This ought to shortcut and directly reply,
  // hence we don't expect an upstream request.
  Envoy::IntegrationStreamDecoderPtr response = getResponse("bad_json", false);
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_EQ(
      response->body(),
      "time-tracking didn't understand the request: Error merging json config: Unable to parse "
      "JSON as proto (INVALID_ARGUMENT:Unexpected token.\nbad_json\n^): bad_json");
  // Send an empty config header, which ought to trigger failure mode as well.
  response = getResponse("", false);
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_EQ(
      response->body(),
      "time-tracking didn't understand the request: Error merging json config: Unable to "
      "parse JSON as proto (INVALID_ARGUMENT:Unexpected end of string. Expected a value.\n\n^): ");
}

class HttpTimeTrackingFilterConfigTest : public testing::Test,
                                         public Envoy::Event::TestUsingSimulatedTime {};

TEST_F(HttpTimeTrackingFilterConfigTest, GetElapsedNanosSinceLastRequest) {
  Envoy::Event::SimulatedTimeSystem& time_system = simTime();
  Server::HttpTimeTrackingFilterConfig config({});
  EXPECT_EQ(config.getElapsedNanosSinceLastRequest(time_system), 0);
  time_system.setMonotonicTime(1ns);
  EXPECT_EQ(config.getElapsedNanosSinceLastRequest(time_system), 1);
  time_system.setMonotonicTime(1s + 1ns);
  EXPECT_EQ(config.getElapsedNanosSinceLastRequest(time_system), 1e9);
  time_system.setMonotonicTime(60s + 1s + 1ns);
  EXPECT_EQ(config.getElapsedNanosSinceLastRequest(time_system), 60 * 1e9);
}

} // namespace
} // namespace Nighthawk
