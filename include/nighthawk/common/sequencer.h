#pragma once

#include <functional>
#include <memory>

#include "envoy/common/pure.h"

#include "nighthawk/common/operation_callback.h"
#include "nighthawk/common/rate_limiter.h"
#include "nighthawk/common/statistic.h"

namespace Nighthawk {

using SequencerTarget = std::function<bool(OperationCallback)>;

/**
 * Abstract Sequencer interface.
 */
class Sequencer {
public:
  virtual ~Sequencer() = default;

  /**
   * Starts the sequencer.
   */
  virtual void start() PURE;

  /**
   * Wait until the sequencer has finished.
   */
  virtual void waitForCompletion() PURE;

  /**
   * @return std::chrono::nanoseconds actual sequencer execution duration.
   */
  virtual std::chrono::nanoseconds executionDuration() const PURE;

  /**
   * @return RateLimiter& reference to the rate limiter associated to this sequencer.
   */
  virtual const RateLimiter& rate_limiter() const PURE;

  /**
   * @return double an up-to-date completions per second rate.
   */
  virtual double completionsPerSecond() const PURE;

  /**
   * Gets the statistics, keyed by id.
   *
   * @return StatisticPtrMap A map of Statistics keyed by id.
   * Will contain statistics for latency (between calling the SequencerTarget and observing its
   * callback) and blocking (tracks time spend waiting on SequencerTarget while it returns false, In
   * other words, time spend while the Sequencer is idle and not blocked by a rate limiter).
   */
  virtual StatisticPtrMap statistics() const PURE;
};

using SequencerPtr = std::unique_ptr<Sequencer>;

} // namespace Nighthawk
