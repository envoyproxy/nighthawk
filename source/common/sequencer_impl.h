#pragma once

#include "common/common/logger.h"

#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "envoy/event/dispatcher.h"
#include "envoy/thread/thread.h"

#include "nighthawk/common/platform_util.h"
#include "nighthawk/common/rate_limiter.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/statistic.h"

namespace Nighthawk {

namespace {

using namespace std::chrono_literals;

constexpr std::chrono::milliseconds EnvoyTimerMinResolution = 1ms;

} // namespace

using SequencerTarget = std::function<bool(std::function<void()>)>;

/**
 * The Sequencer will drive calls to the SequencerTarget at a pace indicated by the associated
 * RateLimiter. The contract with the target is that it will call the provided callback when it is
 * ready. The target will return true if it was able to proceed, or false if a retry is warranted at
 * a later time (because of being out of required resources, for example).
 * Note that owner of SequencerTarget must outlive the SequencerImpl to avoid use-after-free.
 * Also, the Sequencer implementation is a single-shot design. The general usage pattern is:
 *   SequencerImpl sequencer(...)
 *   sequencer.start();
 *   sequencer.waitForCompletion();
 */
class SequencerImpl : public Sequencer, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  SequencerImpl(const PlatformUtil& platform_util, Envoy::Event::Dispatcher& dispatcher,
                Envoy::TimeSource& time_source, Envoy::MonotonicTime start_time,
                RateLimiterPtr&& rate_limiter, SequencerTarget target,
                StatisticPtr&& latency_statistic, StatisticPtr&& blocked_statistic,
                std::chrono::microseconds duration, std::chrono::microseconds grace_timeout);

  /**
   * Starts the Sequencer. Should be followed up with a call to waitForCompletion().
   */
  void start() override;

  /**
   * Blocking call that waits for the Sequencer flow to terminate. Start() must have been called
   * before this.
   */
  void waitForCompletion() override;

  // TODO(oschaaf): calling this after stop() will return broken/unexpected results.
  double completionsPerSecond() const override {
    const double usec = std::chrono::duration_cast<std::chrono::microseconds>(
                            time_source_.monotonicTime() - start_time_)
                            .count();

    return usec == 0 ? 0 : ((targets_completed_ / usec) * 1000000);
  }

  virtual StatisticPtrMap statistics() const override;

  const Statistic& blockedStatistic() const { return *blocked_statistic_; }
  const Statistic& latencyStatistic() const { return *latency_statistic_; }

protected:
  /**
   * Run is called initially by start() and thereafter by two timers:
   *  - a periodic one running at a 1 ms resolution (the current minimum)
   *  - one to spin on calls to run().
   *
   * Spinning is performed when the Sequencer implementation considers itself idle, where "idle" is
   * defined as:
   * - All benchmark target calls have reported back
   * - Either the rate limiter or the benchmark target is prohibiting initiation of the next
   * benchmark target call.
   *
   * The spinning is performed to improve timelyness when initiating
   * calls to the benchmark targets, and observational data also shows significant improvement of
   * actually latency measurement (most pronounced on non-tuned systems). As a side-effect, spinning
   * keeps the CPU busy, preventing C-state frequency changes. Systems with appropriately cooled
   * processors should not be impacted by thermal throttling. When thermal throttling does occur, it
   * makes sense to first warm up the system to get it into a steady state regarding processor
   * frequency.
   *
   * For more context on the current implementation of how we spin, see the the review discussion:
   * https://github.com/envoyproxy/envoy-perf/pull/49#discussion_r259133387
   *
   * @param from_periodic_timer Indicates if we this is called from the periodic timer.
   * Used to determine if re-enablement of the periodic timer should be performed before returning.
   */
  void run(bool from_periodic_timer);
  void scheduleRun();
  void stop(bool timed_out);
  void unblockAndUpdateStatisticIfNeeded(const Envoy::MonotonicTime& now);
  void updateStartBlockingTimeIfNeeded();

private:
  SequencerTarget target_;
  const PlatformUtil& platform_util_;
  Envoy::Event::Dispatcher& dispatcher_;
  Envoy::TimeSource& time_source_;
  std::unique_ptr<RateLimiter> rate_limiter_;
  StatisticPtr latency_statistic_;
  StatisticPtr blocked_statistic_;
  Envoy::Event::TimerPtr periodic_timer_;
  Envoy::Event::TimerPtr spin_timer_;
  std::chrono::microseconds duration_;
  std::chrono::microseconds grace_timeout_;
  Envoy::MonotonicTime start_time_;
  uint64_t targets_initiated_;
  uint64_t targets_completed_;
  bool running_;
  bool blocked_;
  Envoy::MonotonicTime blocked_start_;
};

} // namespace Nighthawk
