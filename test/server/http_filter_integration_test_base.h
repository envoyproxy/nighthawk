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
   * @brief Construct a new HttpFilterIntegrationTestBase instance.
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
  void setup(const std::string& config);

  /**
   * @brief Fetch a response with the default request headers, and set up a fake upstream to supply
   * the response.
   *
   * @return Envoy::IntegrationStreamDecoderPtr Pointer to the integration stream decoder, which can
   * be used to inspect the response.
   */
  Envoy::IntegrationStreamDecoderPtr getResponseFromUpstream();

  /**
   * @brief Fetches a response with request-level configuration set in the request header, and set
   * up a fake upstream to supply the response.
   *
   * @param request_level_config Configuration to be delivered by request header. For example
   * "{{response_body_size:1024}".
   * @return Envoy::IntegrationStreamDecoderPtr Pointer to the integration stream decoder, which can
   * be used to inspect the response.
   */
  Envoy::IntegrationStreamDecoderPtr
  getResponseFromUpstream(absl::string_view request_level_config);

  /**
   * @brief Fetches a response with request-level configuration set in the request header. The
   * extension under test should supply the response.
   *
   * @param request_level_config Configuration to be delivered by request header. For example
   * "{{response_body_size:1024}".
   * @return Envoy::IntegrationStreamDecoderPtr Pointer to the integration stream decoder, which can
   * be used to inspect the response.
   */
  Envoy::IntegrationStreamDecoderPtr
  getResponseFromExtension(absl::string_view request_level_config);

  /**
   * @brief Fetch a response using the provided request headers, and set up a fake upstream to
   * supply a response.
   *
   * @param request_headers Supply a full set of request headers. If the request method is set to
   * POST, an entity body will be send after the request headers.
   * @return Envoy::IntegrationStreamDecoderPtr Pointer to the integration stream decoder, which can
   * be used to inspect the response.
   */
  Envoy::IntegrationStreamDecoderPtr
  getResponseFromUpstream(const Envoy::Http::TestRequestHeaderMapImpl& request_headers);

  /**
   * @brief Fetch a response using the provided request headers. The extension under test must
   * supply a response.
   *
   * @param request_headers Supply a full set of request headers. If the request method is set to
   * POST, an entity body will be send after the request headers.
   * @return Envoy::IntegrationStreamDecoderPtr Pointer to the integration stream decoder, which can
   * be used to inspect the response.
   */
  Envoy::IntegrationStreamDecoderPtr
  getResponseFromExtension(const Envoy::Http::TestRequestHeaderMapImpl& request_headers);

private:
  /**
   * @brief Fetches a response with request-level configuration set in the request header.
   *
   * @param request_level_config Configuration to be delivered by request header. For example
   * "{{response_body_size:1024}".
   * @param setup_for_upstream_request Set to true iff the filter extension under test is expected
   * to short-circuit and supply a response directly. For example because it couldn't parse the
   * supplied request-level configuration. Otherwise this should be set to true, and a stock
   * response will be yielded by the integration test server through an upstream request.
   * @return Envoy::IntegrationStreamDecoderPtr Pointer to the integration stream decoder, which can
   * be used to inspect the response.
   */
  Envoy::IntegrationStreamDecoderPtr getResponse(absl::string_view request_level_config,
                                                 bool setup_for_upstream_request);

  /**
   * @brief Fetch a response using the provided request headers.
   *
   * @param request_headers Supply a full set of request headers. If the request method is set to
   * POST, an entity body will be send after the request headers.
   * @param setup_for_upstream_request Set to true iff the filter extension under test is expected
   * to short-circuit and supply a response directly. For example because it couldn't parse the
   * supplied request-level configuration. Otherwise this should be set to true, and a stock
   * response will be yielded by the integration test server through an upstream request.
   * @return Envoy::IntegrationStreamDecoderPtr Pointer to the integration stream decoder, which can
   * be used to inspect the response.
   */
  Envoy::IntegrationStreamDecoderPtr
  getResponse(const Envoy::Http::TestRequestHeaderMapImpl& request_headers,
              bool setup_for_upstream_request);

  const Envoy::Http::TestRequestHeaderMapImpl request_headers_;
};

} // namespace Nighthawk
