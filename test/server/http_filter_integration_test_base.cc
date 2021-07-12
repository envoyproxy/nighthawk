#include "test/server/http_filter_integration_test_base.h"

#include "source/server/well_known_headers.h"

#include "gtest/gtest.h"

namespace Nighthawk {

HttpFilterIntegrationTestBase::HttpFilterIntegrationTestBase(
    Envoy::Network::Address::IpVersion ip_version)
    : HttpIntegrationTest(Envoy::Http::CodecClient::Type::HTTP1, ip_version),
      request_headers_({{":method", "GET"}, {":path", "/"}, {":authority", "host"}}) {}

void HttpFilterIntegrationTestBase::initializeFilterConfiguration(absl::string_view config) {
  config_helper_.addFilter(std::string(config));
  HttpIntegrationTest::initialize();
}

void HttpFilterIntegrationTestBase::setRequestLevelConfiguration(
    absl::string_view request_level_config) {
  setRequestHeader(Server::TestServer::HeaderNames::get().TestServerConfig, request_level_config);
}

void HttpFilterIntegrationTestBase::appendRequestLevelConfiguration(
    absl::string_view request_level_config) {
  appendRequestHeader(Server::TestServer::HeaderNames::get().TestServerConfig,
                      request_level_config);
}

void HttpFilterIntegrationTestBase::switchToPostWithEntityBody() {
  setRequestHeader(Envoy::Http::Headers::get().Method,
                   Envoy::Http::Headers::get().MethodValues.Post);
}

void HttpFilterIntegrationTestBase::setRequestHeader(
    const Envoy::Http::LowerCaseString& header_name, absl::string_view header_value) {
  request_headers_.setCopy(header_name, header_value);
}

void HttpFilterIntegrationTestBase::appendRequestHeader(
    const Envoy::Http::LowerCaseString& header_name, absl::string_view header_value) {
  request_headers_.addCopy(header_name, header_value);
}

Envoy::IntegrationStreamDecoderPtr
HttpFilterIntegrationTestBase::getResponse(ResponseOrigin expected_origin) {
  cleanupUpstreamAndDownstream();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  Envoy::IntegrationStreamDecoderPtr response;
  const bool is_post = request_headers_.Method()->value().getStringView() ==
                       Envoy::Http::Headers::get().MethodValues.Post;
  // Upon observing a POST request method, we inject a content body, as promised in the header file.
  // This is useful, because emitting an entity body will hit distinct code in extensions. Hence we
  // facilitate that.
  const uint64_t request_body_size = is_post ? 1024 : 0;
  // An extension can either act as an origin and synthesize a reply, or delegate that
  // responsibility to an upstream. This behavior may change from request to request. For example,
  // an extension is designed to transform input from an upstream, may start acting as an origin on
  // misconfiguration.
  if (expected_origin == ResponseOrigin::UPSTREAM) {
    response = sendRequestAndWaitForResponse(request_headers_, request_body_size,
                                             default_response_headers_, /*response_body_size*/ 0);
  } else {
    if (is_post) {
      response = codec_client_->makeRequestWithBody(request_headers_, request_body_size);
    } else {
      response = codec_client_->makeHeaderOnlyRequest(request_headers_);
    }
  }
  return response;
}

} // namespace Nighthawk
