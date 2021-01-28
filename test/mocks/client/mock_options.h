#pragma once

#include "nighthawk/client/options.h"

#include "absl/types/optional.h"
#include "gmock/gmock.h"

namespace Nighthawk {
namespace Client {

class MockOptions : public Options {
public:
  MockOptions();
  MOCK_CONST_METHOD0(requestsPerSecond, uint32_t());
  MOCK_CONST_METHOD0(connections, uint32_t());
  MOCK_CONST_METHOD0(duration, std::chrono::seconds());
  MOCK_CONST_METHOD0(timeout, std::chrono::seconds());
  MOCK_CONST_METHOD0(uri, absl::optional<std::string>());
  MOCK_CONST_METHOD0(h2, bool());
  MOCK_CONST_METHOD0(concurrency, std::string());
  MOCK_CONST_METHOD0(verbosity, nighthawk::client::Verbosity::VerbosityOptions());
  MOCK_CONST_METHOD0(outputFormat, nighthawk::client::OutputFormat::OutputFormatOptions());
  MOCK_CONST_METHOD0(prefetchConnections, bool());
  MOCK_CONST_METHOD0(burstSize, uint32_t());
  MOCK_CONST_METHOD0(addressFamily, nighthawk::client::AddressFamily::AddressFamilyOptions());
  MOCK_CONST_METHOD0(requestMethod, envoy::config::core::v3::RequestMethod());
  MOCK_CONST_METHOD0(requestHeaders, std::vector<std::string>());
  MOCK_CONST_METHOD0(requestBodySize, uint32_t());
  MOCK_CONST_METHOD0(tlsContext,
                     envoy::extensions::transport_sockets::tls::v3::UpstreamTlsContext&());
  MOCK_CONST_METHOD0(transportSocket, absl::optional<envoy::config::core::v3::TransportSocket>&());
  MOCK_CONST_METHOD0(maxPendingRequests, uint32_t());
  MOCK_CONST_METHOD0(maxActiveRequests, uint32_t());
  MOCK_CONST_METHOD0(maxRequestsPerConnection, uint32_t());
  MOCK_CONST_METHOD0(toCommandLineOptions, CommandLineOptionsPtr());
  MOCK_CONST_METHOD0(sequencerIdleStrategy,
                     nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions());
  MOCK_CONST_METHOD0(requestSource, std::string());
  MOCK_CONST_METHOD0(requestSourcePluginConfig,
                     absl::optional<envoy::config::core::v3::TypedExtensionConfig>&());
  MOCK_CONST_METHOD0(trace, std::string());
  MOCK_CONST_METHOD0(
      h1ConnectionReuseStrategy,
      nighthawk::client::H1ConnectionReuseStrategy::H1ConnectionReuseStrategyOptions());
  MOCK_CONST_METHOD0(terminationPredicates, TerminationPredicateMap());
  MOCK_CONST_METHOD0(failurePredicates, TerminationPredicateMap());
  MOCK_CONST_METHOD0(openLoop, bool());
  MOCK_CONST_METHOD0(jitterUniform, std::chrono::nanoseconds());
  MOCK_CONST_METHOD0(nighthawkService, std::string());
  MOCK_CONST_METHOD0(h2UseMultipleConnections, bool());
  MOCK_CONST_METHOD0(multiTargetEndpoints, std::vector<nighthawk::client::MultiTarget::Endpoint>());
  MOCK_CONST_METHOD0(multiTargetPath, std::string());
  MOCK_CONST_METHOD0(multiTargetUseHttps, bool());
  MOCK_CONST_METHOD0(labels, std::vector<std::string>());
  MOCK_CONST_METHOD0(simpleWarmup, bool());
  MOCK_CONST_METHOD0(noDuration, bool());
  MOCK_CONST_METHOD0(statsSinks, std::vector<envoy::config::metrics::v3::StatsSink>());
  MOCK_CONST_METHOD0(statsFlushInterval, uint32_t());
  MOCK_CONST_METHOD0(responseHeaderWithLatencyInput, std::string());
  MOCK_CONST_METHOD0(allowEnvoyDeprecatedV2Api, bool());
  MOCK_CONST_METHOD0(scheduled_start, absl::optional<Envoy::SystemTime>());
};

} // namespace Client
} // namespace Nighthawk
