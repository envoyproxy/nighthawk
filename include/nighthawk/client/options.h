#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "envoy/api/v2/cds.pb.h"
#include "envoy/common/pure.h"

#include "api/client/options.pb.h"

namespace Nighthawk {
namespace Client {

using CommandLineOptionsPtr = std::unique_ptr<nighthawk::client::CommandLineOptions>;

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
  virtual std::string verbosity() const PURE;
  virtual std::string outputFormat() const PURE;
  virtual bool prefetchConnections() const PURE;
  virtual uint32_t burstSize() const PURE;
  virtual std::string addressFamily() const PURE;
  virtual std::string requestMethod() const PURE;
  virtual std::vector<std::string> requestHeaders() const PURE;
  virtual uint32_t requestBodySize() const PURE;
  virtual const envoy::api::v2::auth::UpstreamTlsContext& tlsContext() const PURE;
  virtual uint32_t maxPendingRequests() const PURE;
  virtual uint32_t maxActiveRequests() const PURE;
  virtual uint32_t maxRequestsPerConnection() const PURE;

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
