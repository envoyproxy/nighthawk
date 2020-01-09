#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>

#include "envoy/common/pure.h"
#include "envoy/config/cluster/v3alpha/cluster.pb.h"
#include "envoy/config/core/v3alpha/base.pb.h"

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
  virtual std::string uri() const PURE;
  virtual bool h2() const PURE;
  virtual std::string concurrency() const PURE;
  virtual nighthawk::client::Verbosity::VerbosityOptions verbosity() const PURE;
  virtual nighthawk::client::OutputFormat::OutputFormatOptions outputFormat() const PURE;
  virtual bool prefetchConnections() const PURE;
  virtual uint32_t burstSize() const PURE;
  virtual nighthawk::client::AddressFamily::AddressFamilyOptions addressFamily() const PURE;
  virtual envoy::config::core::v3alpha::RequestMethod requestMethod() const PURE;
  virtual std::vector<std::string> requestHeaders() const PURE;
  virtual uint32_t requestBodySize() const PURE;
  virtual const envoy::extensions::transport_sockets::tls::v3alpha::UpstreamTlsContext&
  tlsContext() const PURE;
  virtual const absl::optional<envoy::config::core::v3alpha::TransportSocket>&
  transportSocket() const PURE;
  virtual uint32_t maxPendingRequests() const PURE;
  virtual uint32_t maxActiveRequests() const PURE;
  virtual uint32_t maxRequestsPerConnection() const PURE;
  virtual nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions
  sequencerIdleStrategy() const PURE;
  virtual std::string trace() const PURE;
  virtual nighthawk::client::H1ConnectionReuseStrategy::H1ConnectionReuseStrategyOptions
  h1ConnectionReuseStrategy() const PURE;
  virtual TerminationPredicateMap terminationPredicates() const PURE;
  virtual TerminationPredicateMap failurePredicates() const PURE;
  virtual bool openLoop() const PURE;
  virtual std::chrono::nanoseconds jitterUniform() const PURE;
  virtual std::vector<std::string> labels() const PURE;
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
