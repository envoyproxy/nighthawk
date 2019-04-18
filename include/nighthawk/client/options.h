#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "envoy/common/pure.h"

#include "source/client/options.pb.h"

namespace Nighthawk {
namespace Client {

using CommandLineOptionsPtr = std::unique_ptr<nighthawk::client::CommandLineOptions>;

/**
 * Abstract options interface.
 */
class Options {
public:
  virtual ~Options() = default;

  virtual uint64_t requestsPerSecond() const PURE;
  virtual uint64_t connections() const PURE;
  virtual std::chrono::seconds duration() const PURE;
  virtual std::chrono::seconds timeout() const PURE;
  virtual std::string uri() const PURE;
  virtual bool h2() const PURE;
  virtual std::string concurrency() const PURE;
  virtual std::string verbosity() const PURE;
  virtual std::string outputFormat() const PURE;
  virtual bool prefetchConnections() const PURE;

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
