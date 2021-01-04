#pragma once

#include "nighthawk/client/output_collector.h"

#include "absl/status/status.h"

namespace Nighthawk {
namespace Client {

/**
 * Process context is shared between the CLI and grpc service. It is capable of executing
 * a full Nighthawk test run.
 */
class Process {
public:
  virtual ~Process() = default;

  /**
   * @param collector used to transform output into the desired format.
   * @return OK if execution succeeded or was cancelled, otherwise error details.
   */
  virtual absl::Status run(OutputCollector& collector) PURE;

  /**
   * Shuts down the worker. Mandatory call before destructing.
   */
  virtual void shutdown() PURE;

  /**
   * Will request all workers to cancel execution asap.
   */
  virtual bool requestExecutionCancellation() PURE;
};

using ProcessPtr = std::unique_ptr<Process>;

} // namespace Client
} // namespace Nighthawk
