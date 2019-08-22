#pragma once

#include <memory>

#include "nighthawk/common/statistic.h"

#include "envoy/common/pure.h"

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
};

using WorkerPtr = std::unique_ptr<Worker>;

} // namespace Nighthawk
