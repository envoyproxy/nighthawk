#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>

#include "nighthawk/client/options.h"
#include "nighthawk/common/exception.h"

namespace Nighthawk {
namespace Client {

class OptionsImpl : public Options {
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
  std::string verbosity() const override { return verbosity_; };
  std::string outputFormat() const override { return output_format_; };
  bool prefetchConnections() const override { return prefetch_connections_; }
  uint32_t burstSize() const override { return burst_size_; }
  AddressFamilyOptions addressFamily() const override { return address_family_; };
  std::string requestMethod() const override { return request_method_; };
  std::vector<std::string> requestHeaders() const override { return request_headers_; };
  uint32_t requestBodySize() const override { return request_body_size_; };
  const envoy::api::v2::auth::UpstreamTlsContext& tlsContext() const override {
    return tls_context_;
  };
  uint32_t maxPendingRequests() const override { return max_pending_requests_; }
  uint32_t maxActiveRequests() const override { return max_active_requests_; }
  uint32_t maxRequestsPerConnection() const override { return max_requests_per_connection_; }
  std::string sequencerIdleStrategy() const override { return sequencer_idle_strategy_; }

private:
  void setNonTrivialDefaults();
  void validate() const;

  uint32_t requests_per_second_{5};
  uint32_t connections_{1};
  uint32_t duration_{5};
  uint32_t timeout_{30};
  std::string uri_;
  bool h2_{false};
  std::string concurrency_;
  std::string verbosity_;
  std::string output_format_;
  bool prefetch_connections_{false};
  uint32_t burst_size_{0};
  AddressFamilyOptions address_family_;
  std::string request_method_;
  std::vector<std::string> request_headers_;
  uint32_t request_body_size_{0};
  envoy::api::v2::auth::UpstreamTlsContext tls_context_;
  uint32_t max_pending_requests_{1};
  uint32_t max_active_requests_{largest_acceptable_uint32_option_value};
  uint32_t max_requests_per_connection_{largest_acceptable_uint32_option_value};
  std::string sequencer_idle_strategy_;
};

} // namespace Client
} // namespace Nighthawk
