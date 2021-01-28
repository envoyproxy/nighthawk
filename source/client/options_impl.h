#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>

#include "nighthawk/client/options.h"
#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/common/logger.h"

#include "absl/types/optional.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace Client {

class OptionsImpl : public Options, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  // We cap on negative values. TCLAP accepts negative values which we will get here as very
  // large values. We just cap values, hoping we catch accidental wraparound to a reasonable extent.
  static constexpr uint32_t largest_acceptable_uint32_option_value = UINT32_MAX - 30000;

  OptionsImpl(int argc, const char* const* argv);
  OptionsImpl(const nighthawk::client::CommandLineOptions& options);
  Client::CommandLineOptionsPtr toCommandLineOptions() const override;

  uint32_t requestsPerSecond() const override { return requests_per_second_; }
  uint32_t connections() const override { return connections_; }
  std::chrono::seconds duration() const override { return std::chrono::seconds(duration_); }
  std::chrono::seconds timeout() const override { return std::chrono::seconds(timeout_); }
  absl::optional<std::string> uri() const override { return uri_; }
  bool h2() const override { return h2_; }
  std::string concurrency() const override { return concurrency_; }
  nighthawk::client::Verbosity::VerbosityOptions verbosity() const override { return verbosity_; };
  nighthawk::client::OutputFormat::OutputFormatOptions outputFormat() const override {
    return output_format_;
  };
  bool prefetchConnections() const override { return prefetch_connections_; }
  uint32_t burstSize() const override { return burst_size_; }
  nighthawk::client::AddressFamily::AddressFamilyOptions addressFamily() const override {
    return address_family_;
  };
  envoy::config::core::v3::RequestMethod requestMethod() const override { return request_method_; };
  std::vector<std::string> requestHeaders() const override { return request_headers_; };
  uint32_t requestBodySize() const override { return request_body_size_; };
  const envoy::extensions::transport_sockets::tls::v3::UpstreamTlsContext&
  tlsContext() const override {
    return tls_context_;
  };
  const absl::optional<envoy::config::core::v3::TransportSocket>& transportSocket() const override {
    return transport_socket_;
  }
  uint32_t maxPendingRequests() const override { return max_pending_requests_; }
  uint32_t maxActiveRequests() const override { return max_active_requests_; }
  uint32_t maxRequestsPerConnection() const override { return max_requests_per_connection_; }
  nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions
  sequencerIdleStrategy() const override {
    return sequencer_idle_strategy_;
  }
  std::string requestSource() const override { return request_source_; }
  const absl::optional<envoy::config::core::v3::TypedExtensionConfig>&
  requestSourcePluginConfig() const override {
    return request_source_plugin_config_;
  }

  std::string trace() const override { return trace_; }
  nighthawk::client::H1ConnectionReuseStrategy::H1ConnectionReuseStrategyOptions
  h1ConnectionReuseStrategy() const override {
    return experimental_h1_connection_reuse_strategy_;
  }
  TerminationPredicateMap terminationPredicates() const override { return termination_predicates_; }
  TerminationPredicateMap failurePredicates() const override { return failure_predicates_; }
  bool openLoop() const override { return open_loop_; }

  std::chrono::nanoseconds jitterUniform() const override { return jitter_uniform_; }
  std::string nighthawkService() const override { return nighthawk_service_; }
  bool h2UseMultipleConnections() const override { return h2_use_multiple_connections_; }
  std::vector<std::string> labels() const override { return labels_; };

  std::vector<nighthawk::client::MultiTarget::Endpoint> multiTargetEndpoints() const override {
    return multi_target_endpoints_;
  }
  std::string multiTargetPath() const override { return multi_target_path_; }
  bool multiTargetUseHttps() const override { return multi_target_use_https_; }
  bool simpleWarmup() const override { return simple_warmup_; }
  bool noDuration() const override { return no_duration_; }
  std::vector<envoy::config::metrics::v3::StatsSink> statsSinks() const override {
    return stats_sinks_;
  }
  uint32_t statsFlushInterval() const override { return stats_flush_interval_; }
  std::string responseHeaderWithLatencyInput() const override {
    return latency_response_header_name_;
  };
  bool allowEnvoyDeprecatedV2Api() const override { return allow_envoy_deprecated_v2_api_; }
  absl::optional<Envoy::SystemTime> scheduled_start() const override { return scheduled_start_; }

private:
  void parsePredicates(const TCLAP::MultiArg<std::string>& arg,
                       TerminationPredicateMap& predicates);
  void setNonTrivialDefaults();
  void validate() const;
  Client::CommandLineOptionsPtr toCommandLineOptionsInternal() const;

  uint32_t requests_per_second_{5};
  uint32_t connections_{100};
  uint32_t duration_{5};
  uint32_t timeout_{30};
  absl::optional<std::string> uri_;
  bool h2_{false};
  std::string concurrency_;
  nighthawk::client::Verbosity::VerbosityOptions verbosity_{nighthawk::client::Verbosity::WARN};
  nighthawk::client::OutputFormat::OutputFormatOptions output_format_{
      nighthawk::client::OutputFormat::JSON};
  bool prefetch_connections_{false};
  uint32_t burst_size_{0};
  nighthawk::client::AddressFamily::AddressFamilyOptions address_family_{
      nighthawk::client::AddressFamily::AUTO};
  envoy::config::core::v3::RequestMethod request_method_{
      envoy::config::core::v3::RequestMethod::GET};
  std::vector<std::string> request_headers_;
  uint32_t request_body_size_{0};
  envoy::extensions::transport_sockets::tls::v3::UpstreamTlsContext tls_context_;
  absl::optional<envoy::config::core::v3::TransportSocket> transport_socket_;
  absl::optional<envoy::config::core::v3::TypedExtensionConfig> request_source_plugin_config_;

  uint32_t max_pending_requests_{0};
  // This default is based the minimum recommendation for SETTINGS_MAX_CONCURRENT_STREAMS over at
  // https://tools.ietf.org/html/rfc7540#section-6.5.2
  uint32_t max_active_requests_{100};
  uint32_t max_requests_per_connection_{largest_acceptable_uint32_option_value};
  nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions sequencer_idle_strategy_{
      nighthawk::client::SequencerIdleStrategy::SPIN};
  std::string request_source_;
  std::string trace_;
  nighthawk::client::H1ConnectionReuseStrategy::H1ConnectionReuseStrategyOptions
      experimental_h1_connection_reuse_strategy_{nighthawk::client::H1ConnectionReuseStrategy::MRU};
  TerminationPredicateMap termination_predicates_;
  TerminationPredicateMap failure_predicates_;
  bool open_loop_{false};
  std::chrono::nanoseconds jitter_uniform_;
  std::string nighthawk_service_;
  bool h2_use_multiple_connections_{false};
  std::vector<nighthawk::client::MultiTarget::Endpoint> multi_target_endpoints_;
  std::string multi_target_path_;
  bool multi_target_use_https_{false};
  std::vector<std::string> labels_;
  bool simple_warmup_{false};
  bool no_duration_{false};
  std::vector<envoy::config::metrics::v3::StatsSink> stats_sinks_;
  uint32_t stats_flush_interval_{5};
  std::string latency_response_header_name_;
  bool allow_envoy_deprecated_v2_api_{false};
  absl::optional<Envoy::SystemTime> scheduled_start_;
};

} // namespace Client
} // namespace Nighthawk
