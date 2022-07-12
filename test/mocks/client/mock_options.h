#pragma once

#include "nighthawk/client/options.h"

#include "absl/types/optional.h"
#include "gmock/gmock.h"

namespace Nighthawk {
namespace Client {

class MockOptions : public Options {
public:
  MockOptions();
  MOCK_METHOD(uint32_t, requestsPerSecond, (), (const, override));
  MOCK_METHOD(uint32_t, connections, (), (const, override));
  MOCK_METHOD(std::chrono::seconds, duration, (), (const, override));
  MOCK_METHOD(std::chrono::seconds, timeout, (), (const, override));
  MOCK_METHOD(absl::optional<std::string>, uri, (), (const, override));
  MOCK_METHOD(bool, h2, (), (const));
  MOCK_METHOD(Envoy::Http::Protocol, protocol, (), (const, override));
  MOCK_METHOD(absl::optional<envoy::config::core::v3::Http3ProtocolOptions>&, http3ProtocolOptions,
              (), (const, override));
  MOCK_METHOD(std::string, concurrency, (), (const, override));
  MOCK_METHOD(nighthawk::client::Verbosity::VerbosityOptions, verbosity, (), (const, override));
  MOCK_METHOD(nighthawk::client::OutputFormat::OutputFormatOptions, outputFormat, (),
              (const, override));
  MOCK_METHOD(bool, prefetchConnections, (), (const, override));
  MOCK_METHOD(uint32_t, burstSize, (), (const, override));
  MOCK_METHOD(nighthawk::client::AddressFamily::AddressFamilyOptions, addressFamily, (),
              (const, override));
  MOCK_METHOD(envoy::config::core::v3::RequestMethod, requestMethod, (), (const, override));
  MOCK_METHOD(std::vector<std::string>, requestHeaders, (), (const, override));
  MOCK_METHOD(uint32_t, requestBodySize, (), (const, override));
  MOCK_METHOD(envoy::extensions::transport_sockets::tls::v3::UpstreamTlsContext&, tlsContext, (),
              (const, override));
  MOCK_METHOD(absl::optional<envoy::config::core::v3::BindConfig>&, upstreamBindConfig, (),
              (const, override));
  MOCK_METHOD(absl::optional<envoy::config::core::v3::TransportSocket>&, transportSocket, (),
              (const, override));
  MOCK_METHOD(uint32_t, maxPendingRequests, (), (const, override));
  MOCK_METHOD(uint32_t, maxActiveRequests, (), (const, override));
  MOCK_METHOD(uint32_t, maxRequestsPerConnection, (), (const, override));
  MOCK_METHOD(uint32_t, maxConcurrentStreams, (), (const, override));
  MOCK_METHOD(CommandLineOptionsPtr, toCommandLineOptions, (), (const, override));
  MOCK_METHOD(nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions,
              sequencerIdleStrategy, (), (const, override));
  MOCK_METHOD(std::string, requestSource, (), (const, override));
  MOCK_METHOD(absl::optional<envoy::config::core::v3::TypedExtensionConfig>&,
              requestSourcePluginConfig, (), (const, override));
  MOCK_METHOD(std::string, trace, (), (const, override));
  MOCK_METHOD(nighthawk::client::H1ConnectionReuseStrategy::H1ConnectionReuseStrategyOptions,
              h1ConnectionReuseStrategy, (), (const, override));
  MOCK_METHOD(TerminationPredicateMap, terminationPredicates, (), (const, override));
  MOCK_METHOD(TerminationPredicateMap, failurePredicates, (), (const, override));
  MOCK_METHOD(bool, openLoop, (), (const, override));
  MOCK_METHOD(std::chrono::nanoseconds, jitterUniform, (), (const, override));
  MOCK_METHOD(std::string, nighthawkService, (), (const, override));
  MOCK_METHOD(bool, h2UseMultipleConnections, (), (const));
  MOCK_METHOD(std::vector<nighthawk::client::MultiTarget::Endpoint>, multiTargetEndpoints, (),
              (const, override));
  MOCK_METHOD(std::string, multiTargetPath, (), (const, override));
  MOCK_METHOD(bool, multiTargetUseHttps, (), (const, override));
  MOCK_METHOD(std::vector<std::string>, labels, (), (const, override));
  MOCK_METHOD(bool, simpleWarmup, (), (const, override));
  MOCK_METHOD(bool, noDuration, (), (const, override));
  MOCK_METHOD(std::vector<envoy::config::metrics::v3::StatsSink>, statsSinks, (),
              (const, override));
  MOCK_METHOD(uint32_t, statsFlushInterval, (), (const, override));
  MOCK_METHOD(Envoy::ProtobufWkt::Duration, statsFlushIntervalDuration, (), (const, override));
  MOCK_METHOD(std::string, responseHeaderWithLatencyInput, (), (const, override));
  MOCK_METHOD(bool, allowEnvoyDeprecatedV2Api, (), (const));
  MOCK_METHOD(absl::optional<Envoy::SystemTime>, scheduled_start, (), (const, override));
  MOCK_METHOD(absl::optional<std::string>, executionId, (), (const, override));
};

} // namespace Client
} // namespace Nighthawk
