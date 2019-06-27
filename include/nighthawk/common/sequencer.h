
#pragma once

#include <functional>
#include <memory>

#include "envoy/common/pure.h"

#include "nighthawk/common/statistic.h"

namespace Nighthawk {

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

  /**
   * Stops all planned work. Makes pending waitForCompletion() calls return ASAP, disregarding any
   * timeouts.
   */
  virtual void cancel() PURE;
};

using SequencerPtr = std::unique_ptr<Sequencer>;

} // namespace Nighthawk