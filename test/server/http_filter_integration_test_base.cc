#include "test/server/http_filter_integration_test_base.h"

#include "server/well_known_headers.h"

#include "gtest/gtest.h"

namespace Nighthawk {

HttpFilterIntegrationTestBase::HttpFilterIntegrationTestBase(
    Envoy::Network::Address::IpVersion ip_version)
    : HttpIntegrationTest(Envoy::Http::CodecClient::Type::HTTP1, ip_version),
      request_headers_({{":method", "GET"}, {":path", "/"}, {":authority", "host"}}) {}

void HttpFilterIntegrationTestBase::setup(const std::string& config) {
  config_helper_.addFilter(config);
  HttpIntegrationTest::initialize();
}

void HttpFilterIntegrationTestBase::updateRequestLevelConfiguration(
    absl::string_view request_level_config) {
  request_headers_.setCopy(Server::TestServer::HeaderNames::get().TestServerConfig,
                           request_level_config);
}

void HttpFilterIntegrationTestBase::switchToPostWithEntityBody() {
  request_headers_.setCopy(Envoy::Http::Headers::get().Method,
                           Envoy::Http::Headers::get().MethodValues.Post);
}

Envoy::IntegrationStreamDecoderPtr
HttpFilterIntegrationTestBase::getResponse(ResponseOrigin expected_origin) {
  cleanupUpstreamAndDownstream();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  Envoy::IntegrationStreamDecoderPtr response;
  const bool is_post = request_headers_.Method()->value().getStringView() ==
                       Envoy::Http::Headers::get().MethodValues.Post;
  const uint64_t request_body_size = is_post ? 1024 : 0;
  if (expected_origin == ResponseOrigin::UPSTREAM) {
    response = sendRequestAndWaitForResponse(request_headers_, request_body_size,
                                             default_response_headers_, /*response_body_size*/ 0);
  } else {
    if (is_post) {
      response = codec_client_->makeRequestWithBody(request_headers_, request_body_size);
    } else {
      response = codec_client_->makeHeaderOnlyRequest(request_headers_);
    }
    response->waitForEndStream();
  }
  return response;
}

} // namespace Nighthawk
