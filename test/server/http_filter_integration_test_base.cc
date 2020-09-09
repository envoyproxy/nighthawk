#include "test/server/http_filter_integration_test_base.h"

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

Envoy::IntegrationStreamDecoderPtr HttpFilterIntegrationTestBase::getResponseFromUpstream() {
  return getResponse(request_headers_, true);
}

Envoy::IntegrationStreamDecoderPtr
HttpFilterIntegrationTestBase::getResponseFromUpstream(absl::string_view request_level_config) {
  return getResponse(request_level_config, true);
}

Envoy::IntegrationStreamDecoderPtr
HttpFilterIntegrationTestBase::getResponseFromExtension(absl::string_view request_level_config) {
  return getResponse(request_level_config, false);
}

Envoy::IntegrationStreamDecoderPtr HttpFilterIntegrationTestBase::getResponseFromUpstream(
    const Envoy::Http::TestRequestHeaderMapImpl& request_headers) {
  return getResponse(request_headers, true);
}

Envoy::IntegrationStreamDecoderPtr HttpFilterIntegrationTestBase::getResponseFromExtension(
    const Envoy::Http::TestRequestHeaderMapImpl& request_headers) {
  return getResponse(request_headers, false);
}

Envoy::IntegrationStreamDecoderPtr
HttpFilterIntegrationTestBase::getResponse(absl::string_view request_level_config,
                                           bool setup_for_upstream_request) {
  const Envoy::Http::LowerCaseString key("x-nighthawk-test-server-config");
  Envoy::Http::TestRequestHeaderMapImpl request_headers = request_headers_;
  request_headers.setCopy(key, request_level_config);
  return getResponse(request_headers, setup_for_upstream_request);
}

Envoy::IntegrationStreamDecoderPtr HttpFilterIntegrationTestBase::getResponse(
    const Envoy::Http::TestRequestHeaderMapImpl& request_headers, bool setup_for_upstream_request) {
  cleanupUpstreamAndDownstream();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  Envoy::IntegrationStreamDecoderPtr response;
  const bool is_post = request_headers.Method()->value().getStringView() ==
                       Envoy::Http::Headers::get().MethodValues.Post;
  const uint64_t request_body_size = is_post ? 1024 : 0;
  if (setup_for_upstream_request) {
    response = sendRequestAndWaitForResponse(request_headers, request_body_size,
                                             default_response_headers_, /*response_body_size*/ 0);
  } else {
    if (is_post) {
      response = codec_client_->makeRequestWithBody(request_headers, request_body_size);
    } else {
      response = codec_client_->makeHeaderOnlyRequest(request_headers);
    }
    response->waitForEndStream();
  }
  return response;
}

} // namespace Nighthawk
