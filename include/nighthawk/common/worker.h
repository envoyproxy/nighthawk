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
   * Signals the worker thread to start shutting down, without waiting for it to finish. Idempotent,
   * and implied by shutdown(). Called from the main thread. Allows callers that own multiple
   * workers to signal all of them before joining any, so the workers' shutdown sequences run
   * concurrently. Does not replace shutdown(): callers must still call shutdown() before
   * destruction to join the worker thread.
   */
  virtual void initiateShutdown() PURE;

  /**
   * Shuts down the worker and joins its thread. Must be paired with start, and mandatory. Called
   * from the main thread, after the worker has cleaned up after itself in shutdownThread().
   */
  virtual void shutdown() PURE;

  /**
   * Called on-thread after its designated task finishes. Last chance to clean up while the
   * associated thread is still running.
   */
  virtual void shutdownThread() PURE;
};

using WorkerPtr = std::unique_ptr<Worker>;

} // namespace Nighthawk
