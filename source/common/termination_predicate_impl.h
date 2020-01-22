#include <chrono>

#include "envoy/common/time.h"
#include "envoy/stats/store.h"

#include "nighthawk/common/termination_predicate.h"

namespace Nighthawk {

class TerminationPredicateBaseImpl : public TerminationPredicate {
public:
  TerminationPredicate& link(TerminationPredicatePtr&& child) final {
    RELEASE_ASSERT(linked_child_ == nullptr, "Linked child already set");
    linked_child_ = std::move(child);
    return *linked_child_;
  }
  TerminationPredicate& appendToChain(TerminationPredicatePtr&& child) final {
    if (linked_child_ != nullptr) {
      return linked_child_->appendToChain(std::move(child));
    } else {
      return link(std::move(child));
    }
  }
  TerminationPredicate::Status evaluateChain() final;

private:
  TerminationPredicatePtr linked_child_;
};

/**
 * Predicate which indicates termination iff the passed in duration has expired.
 * time tracking starts at the first call to evaluate().
 */
class DurationTerminationPredicateImpl : public TerminationPredicateBaseImpl {
public:
  DurationTerminationPredicateImpl(Envoy::TimeSource& time_source,
                                   std::chrono::microseconds duration,
                                   const Envoy::MonotonicTime start)
      : time_source_(time_source), start_(start), duration_(duration) {}
  TerminationPredicate::Status evaluate() override;

private:
  Envoy::TimeSource& time_source_;
  const Envoy::MonotonicTime start_;
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