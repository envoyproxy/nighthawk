#include "source/common/sequencer_impl.h"

#include "nighthawk/common/exception.h"
#include "nighthawk/common/platform_util.h"

#include "external/envoy/source/common/common/assert.h"

using namespace std::chrono_literals;

namespace Nighthawk {

SequencerImpl::SequencerImpl(
    const PlatformUtil& platform_util, Envoy::Event::Dispatcher& dispatcher,
    Envoy::TimeSource& time_source, RateLimiterPtr&& rate_limiter, SequencerTarget target,
    StatisticPtr&& latency_statistic, StatisticPtr&& blocked_statistic,
    nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions idle_strategy,
    TerminationPredicatePtr&& termination_predicate, Envoy::Stats::Scope& scope)
    : target_(std::move(target)), platform_util_(platform_util), dispatcher_(dispatcher),
      time_source_(time_source), rate_limiter_(std::move(rate_limiter)),
      latency_statistic_(std::move(latency_statistic)),
      blocked_statistic_(std::move(blocked_statistic)), idle_strategy_(idle_strategy),
      termination_predicate_(std::move(termination_predicate)),
      last_termination_status_(TerminationPredicate::Status::PROCEED),
      scope_(scope.createScope("sequencer.")),
      sequencer_stats_({ALL_SEQUENCER_STATS(POOL_COUNTER(*scope_))}) {
  ASSERT(target_ != nullptr, "No SequencerTarget");
  ASSERT(termination_predicate_ != nullptr, "null termination predicate");
  periodic_timer_ = dispatcher_.createTimer([this]() { run(true); });
  spin_timer_ = dispatcher_.createTimer([this]() { run(false); });
  latency_statistic_->setId("sequencer.callback");
  blocked_statistic_->setId("sequencer.blocking");
}

void SequencerImpl::start() {
  ASSERT(!running_);
  running_ = true;
  // Initiate the periodic timer loop.
  scheduleRun();
  // Immediately run.
  run(false);
}

void SequencerImpl::scheduleRun() { periodic_timer_->enableHRTimer(NighthawkTimerResolution); }

void SequencerImpl::stop(bool failed) {
  ASSERT(running_);
  const double rate = completionsPerSecond();
  if (failed) {
    ENVOY_LOG(error, "Exiting due to failing termination predicate");
    sequencer_stats_.failed_terminations_.inc();
  }
  running_ = false;
  periodic_timer_->disableTimer();
  spin_timer_->disableTimer();
  periodic_timer_.reset();
  spin_timer_.reset();
  dispatcher_.exit();
  unblockAndUpdateStatisticIfNeeded(time_source_.monotonicTime());
  const auto ran_for = std::chrono::duration_cast<std::chrono::milliseconds>(executionDuration());
  ENVOY_LOG(info,
            "Stopping after {} ms. Initiated: {} / Completed: {}. "
            "(Completion rate was {} per second.)",
            ran_for.count(), targets_initiated_, targets_completed_, rate);
}

void SequencerImpl::unblockAndUpdateStatisticIfNeeded(const Envoy::MonotonicTime& now) {
  if (blocked_) {
    blocked_ = false;
    blocked_statistic_->addValue((now - blocked_start_).count());
  }
}

void SequencerImpl::updateStartBlockingTimeIfNeeded() {
  if (!blocked_) {
    blocked_ = true;
    blocked_start_ = time_source_.monotonicTime();
  }
}

void SequencerImpl::run(bool from_periodic_timer) {
  ASSERT(running_);
  // CachedTimeSource relies on the dispatcher's updateApproximateMonotonicTime() /
  // approximateMonotonicTime() for the actual caching. We refresh its stored time-value here so
  // that our cached time source will yield an up-to-date monotonic time sample to work with. This
  // time sample value will then be consistent accross usage here, plus any RateLimiter(s) and/or
  // TerminationPredicate(s) associated to this sequencer that perform computations based on the
  // current time. Having a consistent value accross these usages avoids certain edge cases. For
  // example: A duration predicate determines it is time to stop execution at 1s. A rate limiter
  // determines it is time to release a new request at 1.00001s. While this is a benign example, and
  // would be explainable, it probably has potential to raise eyebrows.
  // More importantly, it may help avoid a class of bugs that could be more serious, depending on
  // functionality (TOC/TOU).
  dispatcher_.updateApproximateMonotonicTime();
  const auto now = time_source_.monotonicTime();

  last_termination_status_ = last_termination_status_ == TerminationPredicate::Status::PROCEED
                                 ? termination_predicate_->evaluateChain()
                                 : last_termination_status_;

  // If we should stop according to termination conditions.
  if (last_termination_status_ != TerminationPredicate::Status::PROCEED) {
    stop(last_termination_status_ == TerminationPredicate::Status::FAIL);
    return;
  }

  while (rate_limiter_->tryAcquireOne()) {
    // The rate limiter says it's OK to proceed and call the target. Let's see if the target is OK
    // with that as well.
    const bool target_could_start = target_([this, now](bool, bool) {
      // Update cached time, as we need an accurate value for latency reporting.
      dispatcher_.updateApproximateMonotonicTime();
      const auto dur = time_source_.monotonicTime() - now;
      latency_statistic_->addValue(dur.count());
      targets_completed_++;
      // Callbacks may fire after stop() is called. When the worker teardown runs the dispatcher,
      // in-flight work might wrap up and fire this callback. By then we wouldn't want to
      // re-enable any timers here.
      if (this->running_) {
        // Immediately schedule us to check again, as chances are we can get on with the next
        // task.
        spin_timer_->enableHRTimer(0ms);
      }
    });
    if (target_could_start) {
      unblockAndUpdateStatisticIfNeeded(now);
      targets_initiated_++;
    } else {
      // This should only happen when we are running in closed-loop mode.The target wasn't able to
      // proceed. Update the rate limiter.
      updateStartBlockingTimeIfNeeded();
      rate_limiter_->releaseOne();
      // Retry later. When all target_ calls have completed we are going to spin until target_
      // stops returning false. Otherwise the periodic timer will wake us up to re-check.
      break;
    }
  }

  if (from_periodic_timer) {
    // Re-schedule the periodic timer if it was responsible for waking up this code.
    scheduleRun();
  } else {
    if (idle_strategy_ == nighthawk::client::SequencerIdleStrategy::SPIN &&
        (targets_initiated_ == targets_completed_)) {
      // We saturated the rate limiter, and there's no outstanding work.
      // That means it looks like we are idle. Spin this event to improve
      // accuracy. As a side-effect, this may help prevent CPU frequency scaling
      // due to c-state changes. But on the other hand it may cause thermal throttling.
      // TODO(oschaaf): Ideally we would have much finer grained timers instead.
      // TODO(oschaaf): Optionize performing this spin loop.
      platform_util_.yieldCurrentThread();
      spin_timer_->enableHRTimer(0ms);
    } else if (idle_strategy_ == nighthawk::client::SequencerIdleStrategy::SLEEP) {
      // optionize sleep duration.
      platform_util_.sleep(50us);
      spin_timer_->enableHRTimer(0ms);
    } // .. else we poll, the periodic timer will be active
  }
}

void SequencerImpl::waitForCompletion() {
  // It's possible that we have already finished when we get here.
  if (running_) {
    dispatcher_.run(Envoy::Event::Dispatcher::RunType::RunUntilExit);
  }
  // We should guarantee the flow terminates, so:
  ASSERT(!running_);
}

StatisticPtrMap SequencerImpl::statistics() const {
  StatisticPtrMap statistics;
  statistics[latency_statistic_->id()] = latency_statistic_.get();
  statistics[blocked_statistic_->id()] = blocked_statistic_.get();
  return statistics;
};

} // namespace Nighthawk
