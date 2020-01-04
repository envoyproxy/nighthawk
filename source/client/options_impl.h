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
  std::string uri() const override { return uri_; }
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
  envoy::api::v2::core::RequestMethod requestMethod() const override { return request_method_; };
  std::vector<std::string> requestHeaders() const override { return request_headers_; };
  uint32_t requestBodySize() const override { return request_body_size_; };
  const envoy::api::v2::auth::UpstreamTlsContext& tlsContext() const override {
    return tls_context_;
  };
  const absl::optional<envoy::api::v2::core::TransportSocket>& transportSocket() const override {
    return transport_socket_;
  }
  uint32_t maxPendingRequests() const override { return max_pending_requests_; }
  uint32_t maxActiveRequests() const override { return max_active_requests_; }
  uint32_t maxRequestsPerConnection() const override { return max_requests_per_connection_; }
  nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions
  sequencerIdleStrategy() const override {
    return sequencer_idle_strategy_;
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
  std::vector<std::string> labels() const override { return labels_; };

private:
  void parsePredicates(const TCLAP::MultiArg<std::string>& arg,
                       TerminationPredicateMap& predicates);
  void setNonTrivialDefaults();
  void validate() const;

  uint32_t requests_per_second_{5};
  uint32_t connections_{100};
  uint32_t duration_{5};
  uint32_t timeout_{30};
  std::string uri_;
  bool h2_{false};
  std::string concurrency_;
  nighthawk::client::Verbosity::VerbosityOptions verbosity_{nighthawk::client::Verbosity::WARN};
  nighthawk::client::OutputFormat::OutputFormatOptions output_format_{
      nighthawk::client::OutputFormat::JSON};
  bool prefetch_connections_{false};
  uint32_t burst_size_{0};
  nighthawk::client::AddressFamily::AddressFamilyOptions address_family_{
      nighthawk::client::AddressFamily::AUTO};
  envoy::api::v2::core::RequestMethod request_method_{envoy::api::v2::core::RequestMethod::GET};
  std::vector<std::string> request_headers_;
  uint32_t request_body_size_{0};
  envoy::api::v2::auth::UpstreamTlsContext tls_context_;
  absl::optional<envoy::api::v2::core::TransportSocket> transport_socket_;
  uint32_t max_pending_requests_{0};
  // This default is based the minimum recommendation for SETTINGS_MAX_CONCURRENT_STREAMS over at
  // https://tools.ietf.org/html/rfc7540#section-6.5.2
  uint32_t max_active_requests_{100};
  uint32_t max_requests_per_connection_{largest_acceptable_uint32_option_value};
  nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions sequencer_idle_strategy_{
      nighthawk::client::SequencerIdleStrategy::SPIN};
  std::string trace_;
  nighthawk::client::H1ConnectionReuseStrategy::H1ConnectionReuseStrategyOptions
      experimental_h1_connection_reuse_strategy_{nighthawk::client::H1ConnectionReuseStrategy::MRU};
  TerminationPredicateMap termination_predicates_;
  TerminationPredicateMap failure_predicates_;
  bool open_loop_{false};
  std::chrono::nanoseconds jitter_uniform_;
  std::vector<std::string> labels_;
};

} // namespace Client
} // namespace Nighthawk
