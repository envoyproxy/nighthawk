#pragma once

#include <string>

#include "external/envoy/test/integration/http_integration.h"

namespace Nighthawk {

/**
 * Base class with shared functionality for testing Nighthawk test server http filter extensions.
 */
class HttpFilterIntegrationTestBase : public Envoy::HttpIntegrationTest {
protected:
  /**
   * Indicate the expected response origin.
   */
  enum class ResponseOrigin { UPSTREAM, EXTENSION };
  /**
   * Construct a new HttpFilterIntegrationTestBase instance.
   *
   * @param ip_version Specify the ip version that the integration test server will use to listen
   * for connections.
   */
  HttpFilterIntegrationTestBase(Envoy::Network::Address::IpVersion ip_version);

  /**
   * We don't override SetUp(): tests using this fixture must call setup() instead.
   * This is to avoid imposing the need to create a fixture per filter configuration.
   *
   * @param config configuration to pass to Envoy::HttpIntegrationTest::config_helper_.addFilter.
   */
  void initializeConfig(absl::string_view config);

  /**
   * Make getResponse send request-level configuration. Test server extensions read that
   * configuration and merge it with their static configuration to determine a final effective
   * configuration. See TestServerConfig in well_known_headers.h for the up to date header name.
   *
   * @param request_level_config Configuration to be delivered by request-header in future calls to
   * getResponse(). For example: "{response_body_size:1024}".
   */
  void updateRequestLevelConfiguration(absl::string_view request_level_config);

  /**
   * Switch getResponse() to use the POST request method with an entity body.
   * Doing so will make tests hit a different code paths in extensions.
   */
  void switchToPostWithEntityBody();

  /**
   * Set a request header value. Overwrites any existing value.
   *
   * @param header_name Name of the request header to set.
   * @param header_value Value to set for the request header.
   */
  void setRequestHeader(const Envoy::Http::LowerCaseString& header_name,
                        absl::string_view header_value);

  /**
   * Fetch a response. The request headers default to a minimal GET, but this may be changed
   * via other methods in this class.
   *
   * @param expected_origin Indicate which component will be expected to reply: the extension or
   * a fake upstream. Will cause a test failure upon mismatch. Can be used to verify that an
   * extension short circuits and directly responds when expected.
   * @return Envoy::IntegrationStreamDecoderPtr Pointer to the integration stream decoder, which can
   * be used to inspect the response.
   */
  Envoy::IntegrationStreamDecoderPtr getResponse(ResponseOrigin expected_origin);

private:
  Envoy::Http::TestRequestHeaderMapImpl request_headers_;
};

} // namespace Nighthawk
