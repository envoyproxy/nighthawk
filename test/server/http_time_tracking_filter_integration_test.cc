#include <chrono>

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
  initializeFilterConfiguration(fmt::format(kProtoConfigTemplate, kDefaultProtoFragment));

  // As the first request doesn't have a prior one, we should not observe a delta.
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::UPSTREAM);
  int64_t latency;
  EXPECT_EQ(
      response->headers().get(Envoy::Http::LowerCaseString(kLatencyResponseHeaderName)).size(), 0);

  // On the second request we should observe a delta.
  response = getResponse(ResponseOrigin::UPSTREAM);
  const Envoy::Http::HeaderMap::GetResult& latency_header =
      response->headers().get(Envoy::Http::LowerCaseString(kLatencyResponseHeaderName));
  ASSERT_EQ(latency_header.size(), 1);
  EXPECT_TRUE(absl::SimpleAtoi(latency_header[0]->value().getStringView(), &latency));
  EXPECT_GT(latency, 0);
}

// Verify expectations with an empty time-tracking configuration.
TEST_P(HttpTimeTrackingIntegrationTest, ReturnsPositiveLatencyForPerRequestConfiguration) {
  initializeFilterConfiguration(fmt::format(kProtoConfigTemplate, ""));
  // As the first request doesn't have a prior one, we should not observe a delta.
  setRequestLevelConfiguration("{}");
  Envoy::IntegrationStreamDecoderPtr response = getResponse(ResponseOrigin::UPSTREAM);
  EXPECT_TRUE(
      response->headers().get(Envoy::Http::LowerCaseString(kLatencyResponseHeaderName)).empty());

  // With request level configuration indicating that the timing header should be emitted,
  // we should be able to observe it.
  setRequestLevelConfiguration(fmt::format("{{{}}}", kDefaultProtoFragment));
  response = getResponse(ResponseOrigin::UPSTREAM);
  const Envoy::Http::HeaderMap::GetResult& latency_header =
      response->headers().get(Envoy::Http::LowerCaseString(kLatencyResponseHeaderName));
  ASSERT_EQ(latency_header.size(), 1);
  int64_t latency;
  EXPECT_TRUE(absl::SimpleAtoi(latency_header[0]->value().getStringView(), &latency));
  // TODO(oschaaf): figure out if we can use simtime here, and verify actual timing matches
  // what we'd expect using that.
  EXPECT_GT(latency, 0);
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
