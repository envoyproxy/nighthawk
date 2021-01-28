#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>

#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "envoy/config/cluster/v3/cluster.pb.h"
#include "envoy/config/core/v3/base.pb.h"
#include "envoy/config/metrics/v3/stats.pb.h"

#include "nighthawk/common/termination_predicate.h"

#include "api/client/options.pb.h"

#include "absl/types/optional.h"

namespace Nighthawk {
namespace Client {

using CommandLineOptionsPtr = std::unique_ptr<nighthawk::client::CommandLineOptions>;
using TerminationPredicateMap = std::map<std::string, uint64_t>;
/**
 * Abstract options interface.
 */
class Options {
public:
  virtual ~Options() = default;

  virtual uint32_t requestsPerSecond() const PURE;
  virtual uint32_t connections() const PURE;
  virtual std::chrono::seconds duration() const PURE;
  virtual std::chrono::seconds timeout() const PURE;
  // URI is absent when the user specified --multi-target-* instead.
  virtual absl::optional<std::string> uri() const PURE;
  virtual bool h2() const PURE;
  virtual std::string concurrency() const PURE;
  virtual nighthawk::client::Verbosity::VerbosityOptions verbosity() const PURE;
  virtual nighthawk::client::OutputFormat::OutputFormatOptions outputFormat() const PURE;
  virtual bool prefetchConnections() const PURE;
  virtual uint32_t burstSize() const PURE;
  virtual nighthawk::client::AddressFamily::AddressFamilyOptions addressFamily() const PURE;
  virtual envoy::config::core::v3::RequestMethod requestMethod() const PURE;
  virtual std::vector<std::string> requestHeaders() const PURE;
  virtual uint32_t requestBodySize() const PURE;
  virtual const envoy::extensions::transport_sockets::tls::v3::UpstreamTlsContext&
  tlsContext() const PURE;
  virtual const absl::optional<envoy::config::core::v3::TransportSocket>&
  transportSocket() const PURE;
  virtual uint32_t maxPendingRequests() const PURE;
  virtual uint32_t maxActiveRequests() const PURE;
  virtual uint32_t maxRequestsPerConnection() const PURE;
  virtual nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions
  sequencerIdleStrategy() const PURE;
  virtual std::string requestSource() const PURE;
  virtual const absl::optional<envoy::config::core::v3::TypedExtensionConfig>&
  requestSourcePluginConfig() const PURE;
  virtual std::string trace() const PURE;
  virtual nighthawk::client::H1ConnectionReuseStrategy::H1ConnectionReuseStrategyOptions
  h1ConnectionReuseStrategy() const PURE;
  virtual TerminationPredicateMap terminationPredicates() const PURE;
  virtual TerminationPredicateMap failurePredicates() const PURE;
  virtual bool openLoop() const PURE;
  virtual std::chrono::nanoseconds jitterUniform() const PURE;
  virtual std::string nighthawkService() const PURE;
  virtual bool h2UseMultipleConnections() const PURE;
  virtual std::vector<nighthawk::client::MultiTarget::Endpoint> multiTargetEndpoints() const PURE;
  virtual std::string multiTargetPath() const PURE;
  virtual bool multiTargetUseHttps() const PURE;
  virtual std::vector<std::string> labels() const PURE;
  virtual bool simpleWarmup() const PURE;
  virtual bool noDuration() const PURE;
  virtual std::vector<envoy::config::metrics::v3::StatsSink> statsSinks() const PURE;
  virtual uint32_t statsFlushInterval() const PURE;
  virtual std::string responseHeaderWithLatencyInput() const PURE;
  virtual bool allowEnvoyDeprecatedV2Api() const PURE;

  virtual absl::optional<Envoy::SystemTime> scheduled_start() const PURE;
  /**
   * Converts an Options instance to an equivalent CommandLineOptions instance in terms of option
   * values.
   * @return CommandLineOptionsPtr
   */
  virtual CommandLineOptionsPtr toCommandLineOptions() const PURE;
};

using OptionsPtr = std::unique_ptr<Options>;

} // namespace Client
} // namespace Nighthawk
