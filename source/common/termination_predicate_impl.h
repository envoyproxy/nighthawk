#include <chrono>

#include "envoy/common/time.h"
#include "envoy/stats/store.h"

#include "nighthawk/common/termination_predicate.h"

namespace Nighthawk {

class TerminationPredicateBaseImpl : public TerminationPredicate {
public:
  void link(TerminationPredicatePtr&& child) final { linked_child_ = std::move(child); }
  TerminationPredicate::Status evaluateChain() final;

private:
  TerminationPredicatePtr linked_child_;
};

class DurationTerminationPredicateImpl : public TerminationPredicateBaseImpl {
public:
  DurationTerminationPredicateImpl(Envoy::TimeSource& time_source,
                                   std::chrono::microseconds duration)
      : time_source_(time_source), start_(time_source.monotonicTime()), duration_(duration) {}
  TerminationPredicate::Status evaluate() override;

private:
  Envoy::TimeSource& time_source_;
  Envoy::MonotonicTime start_;
  std::chrono::microseconds duration_;
};

class StatsCounterAbsoluteThresholdTerminationPredicateImpl : public TerminationPredicateBaseImpl {
public:
  StatsCounterAbsoluteThresholdTerminationPredicateImpl(
      Envoy::Stats::Counter& counter, uint64_t counter_limit,
      TerminationPredicate::Status termination_status)
      : counter_(counter), counter_limit_(counter_limit), termination_status_(termination_status) {}
  TerminationPredicate::Status evaluate() override;

private:
  Envoy::Stats::Counter& counter_;
  const uint64_t counter_limit_;
  const TerminationPredicate::Status termination_status_;
};

} // namespace Nighthawk