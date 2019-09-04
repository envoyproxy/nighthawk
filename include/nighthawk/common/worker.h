#pragma once

#include <memory>

#include "envoy/common/pure.h"

#include "nighthawk/common/statistic.h"

namespace Nighthawk {

/**
 * Interface for a threaded worker.
 */
class Worker {
public:
  virtual ~Worker() = default;

  /**
   * Start the worker thread.
   */
  virtual void start() PURE;

  /**
   * Wait for the worker thread to complete its work.
   */
  virtual void waitForCompletion() PURE;

  /**
   * Shuts down the worker. Must be paired with start,
   * and mandatory.
   */
  virtual void shutDown() PURE;
};

using WorkerPtr = std::unique_ptr<Worker>;

} // namespace Nighthawk
