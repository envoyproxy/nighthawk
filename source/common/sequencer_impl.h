#pragma once

#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "envoy/event/dispatcher.h"

#include "nighthawk/common/operation_callback.h"
#include "nighthawk/common/platform_util.h"
#include "nighthawk/common/rate_limiter.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/termination_predicate.h"

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {

namespace {

using namespace std::chrono_literals;

constexpr std::chrono::milliseconds EnvoyTimerMinResolution = 1ms;

} // namespace

using SequencerTarget = std::function<bool(OperationCallback)>;

using namespace Envoy; // We need this because of macro expectations.

#define ALL_SEQUENCER_STATS(COUNTER) COUNTER(failed_terminations)

struct SequencerStats {
  ALL_SEQUENCER_STATS(GENERATE_COUNTER_STRUCT)
};

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
  SequencerImpl(
      const PlatformUtil& platform_util, Envoy::Event::Dispatcher& dispatcher,
      Envoy::TimeSource& time_source, Envoy::MonotonicTime start_time,
      RateLimiterPtr&& rate_limiter, SequencerTarget target, StatisticPtr&& latency_statistic,
      StatisticPtr&& blocked_statistic,
      nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions idle_strategy,
      TerminationPredicate& termination_predicate, Envoy::Stats::Scope& scope);

  /**
   * Starts the Sequencer. Should be followed up with a call to waitForCompletion().
   */
  void start() override;

  /**
   * Blocking call that waits for the Sequencer flow to terminate. Start() must have been called
   * before this.
   */
  void waitForCompletion() override;

  std::chrono::nanoseconds executionDuration() const override {
    return last_event_time_ - start_time_;
  }

  double completionsPerSecond() const override {
    const double usec =
        std::chrono::duration_cast<std::chrono::microseconds>(last_event_time_ - start_time_)
            .count();

    return usec == 0 ? 0 : ((targets_completed_ / usec) * 1000000);
  }

  StatisticPtrMap statistics() const override;

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
  Envoy::MonotonicTime start_time_;
  Envoy::MonotonicTime last_event_time_;
  uint64_t targets_initiated_{0};
  uint64_t targets_completed_{0};
  bool running_{};
  bool blocked_{};
  Envoy::MonotonicTime blocked_start_;
  nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions idle_strategy_;
  TerminationPredicate& termination_predicate_;
  TerminationPredicate::Status last_termination_status_;
  Envoy::Stats::ScopePtr scope_;
  SequencerStats sequencer_stats_;
};

} // namespace Nighthawk
