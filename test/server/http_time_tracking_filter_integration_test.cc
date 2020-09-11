#include <chrono>

#include "envoy/upstream/cluster_manager.h"
#include "envoy/upstream/upstream.h"

#include "external/envoy/test/common/upstream/utility.h"
#include "external/envoy/test/test_common/simulated_time_system.h"

#include "api/server/response_options.pb.h"
#include "api/server/response_options.pb.validate.h"

#include "server/configuration.h"
#include "server/http_time_tracking_filter.h"

#include "test/server/http_filter_integration_test_base.h"

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
    : public HttpFilterIntegrationTestBase,
      public testing::TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  HttpTimeTrackingIntegrationTest() : HttpFilterIntegrationTestBase(GetParam()){};
};

INSTANTIATE_TEST_SUITE_P(IpVersions, HttpTimeTrackingIntegrationTest,
                         testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

// Verify expectations with static/file-based time-tracking configuration.
TEST_P(HttpTimeTrackingIntegrationTest, ReturnsPositiveLatencyForStaticConfiguration) {
  setup(fmt::format(kProtoConfigTemplate, kDefaultProtoFragment));
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::UPSTREAM);
  int64_t latency;
  const Envoy::Http::HeaderEntry* latency_header_1 =
      response->headers().get(Envoy::Http::LowerCaseString(kLatencyResponseHeaderName));
  EXPECT_EQ(latency_header_1, nullptr);
  response = getResponse(ResponseOrigin::UPSTREAM);
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
  getResponse(ResponseOrigin::UPSTREAM);
  // Send a config request header with an empty / default config. Should be a no-op.
  updateRequestLevelConfiguration("{}");
  getResponse(ResponseOrigin::UPSTREAM);
  // Send a config request header, this should become effective.
  updateRequestLevelConfiguration(fmt::format("{{{}}}", kDefaultProtoFragment));
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::UPSTREAM);
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
  updateRequestLevelConfiguration("bad_json");
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::EXTENSION);
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_EQ(
      response->body(),
      "time-tracking didn't understand the request: Error merging json config: Unable to parse "
      "JSON as proto (INVALID_ARGUMENT:Unexpected token.\nbad_json\n^): bad_json");
  // Send an empty config header, which ought to trigger failure mode as well.
  updateRequestLevelConfiguration("");
  response = getResponse(ResponseOrigin::EXTENSION);
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_EQ(
      response->body(),
      "time-tracking didn't understand the request: Error merging json config: Unable to "
      "parse JSON as proto (INVALID_ARGUMENT:Unexpected end of string. Expected a value.\n\n^): ");
}

// Verify the filter is well-behaved when it comes to requests with an entity body.
// This takes a slightly different code path, so it is important to test this explicitly.
TEST_P(HttpTimeTrackingIntegrationTest, BehaviorWithRequestBody) {
  setup(fmt::format(kProtoConfigTemplate, ""));
  switchToPostWithEntityBody();

  // Post without any request-level configuration. Should succeed.
  getResponse(ResponseOrigin::UPSTREAM);
  updateRequestLevelConfiguration(fmt::format("{{{}}}", kDefaultProtoFragment));
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::UPSTREAM);
  const Envoy::Http::HeaderEntry* latency_header =
      response->headers().get(Envoy::Http::LowerCaseString(kLatencyResponseHeaderName));
  ASSERT_NE(latency_header, nullptr);

  // Post with bad request-level configuration. The extension should respond directly with an
  // error.
  updateRequestLevelConfiguration("bad_json");
  response = getResponse(ResponseOrigin::EXTENSION);
  EXPECT_EQ(Envoy::Http::Utility::getResponseStatus(response->headers()), 500);
  EXPECT_EQ(
      response->body(),
      "time-tracking didn't understand the request: Error merging json config: Unable to parse "
      "JSON as proto (INVALID_ARGUMENT:Unexpected token.\nbad_json\n^): bad_json");
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
