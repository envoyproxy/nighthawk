#pragma once

#include <string>

#include "external/envoy/test/integration/http_integration.h"

namespace Nighthawk {

class HttpFilterIntegrationTestBase : public Envoy::HttpIntegrationTest {
protected:
  HttpFilterIntegrationTestBase(Envoy::Network::Address::IpVersion ip_version);

  // We don't override SetUp(): tests in this file will call setup() instead to avoid having to
  // create a fixture per filter configuration.
  void setup(const std::string& config);

  // Fetches a response with request-level configuration set in the request header.
  Envoy::IntegrationStreamDecoderPtr getResponse(absl::string_view request_level_config,
                                                 bool setup_for_upstream_request = true);

  // Fetches a response with the default request headers, expecting the fake upstream to supply
  // the response.
  Envoy::IntegrationStreamDecoderPtr getResponse();

  // Fetches a response using the provided request headers. When setup_for_upstream_request
  // is true, the expectation will be that an upstream request will be needed to provide a
  // response. If it is set to false, the extension is expected to supply the response, and
  // no upstream request ought to occur.
  Envoy::IntegrationStreamDecoderPtr
  getResponse(const Envoy::Http::TestRequestHeaderMapImpl& request_headers,
              bool setup_for_upstream_request = true);

  const Envoy::Http::TestRequestHeaderMapImpl request_headers_;
};

} // namespace Nighthawk
