#include "test/server/http_filter_integration_test_base.h"

#include "gtest/gtest.h"

namespace Nighthawk {

HttpFilterIntegrationTestBase::HttpFilterIntegrationTestBase(
    Envoy::Network::Address::IpVersion ip_version)
    : HttpIntegrationTest(Envoy::Http::CodecClient::Type::HTTP1, ip_version),
      request_headers_({{":method", "GET"}, {":path", "/"}, {":authority", "host"}}) {}

// We don't override SetUp(): tests in this file will call setup() instead to avoid having to
// create a fixture per filter configuration.
void HttpFilterIntegrationTestBase::setup(const std::string& config) {
  config_helper_.addFilter(config);
  HttpIntegrationTest::initialize();
}

// Fetches a response with request-level configuration set in the request header.
Envoy::IntegrationStreamDecoderPtr
HttpFilterIntegrationTestBase::getResponse(absl::string_view request_level_config,
                                           bool setup_for_upstream_request) {
  const Envoy::Http::LowerCaseString key("x-nighthawk-test-server-config");
  Envoy::Http::TestRequestHeaderMapImpl request_headers = request_headers_;
  request_headers.setCopy(key, request_level_config);
  return getResponse(request_headers, setup_for_upstream_request);
}

// Fetches a response with the default request headers, expecting the fake upstream to supply
// the response.
Envoy::IntegrationStreamDecoderPtr HttpFilterIntegrationTestBase::getResponse() {
  return getResponse(request_headers_);
}

// Fetches a response using the provided request headers. When setup_for_upstream_request
// is true, the expectation will be that an upstream request will be needed to provide a
// response. If it is set to false, the extension is expected to supply the response, and
// no upstream request ought to occur.
Envoy::IntegrationStreamDecoderPtr HttpFilterIntegrationTestBase::getResponse(
    const Envoy::Http::TestRequestHeaderMapImpl& request_headers, bool setup_for_upstream_request) {
  cleanupUpstreamAndDownstream();
  codec_client_ = makeHttpConnection(lookupPort("http"));
  Envoy::IntegrationStreamDecoderPtr response;
  const bool is_post = request_headers.Method()->value().getStringView() == "POST";
  const uint64_t request_body_size = is_post ? 1024 : 0;
  if (setup_for_upstream_request) {
    response = sendRequestAndWaitForResponse(request_headers, request_body_size,
                                             default_response_headers_, 0);
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
