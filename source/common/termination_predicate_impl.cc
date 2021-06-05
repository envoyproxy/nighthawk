#include "source/common/termination_predicate_impl.h"

#include <iostream>

namespace Nighthawk {

TerminationPredicate::Status TerminationPredicateBaseImpl::evaluateChain() {
  auto status = TerminationPredicate::Status::PROCEED;
  if (linked_child_ != nullptr) {
    status = linked_child_->evaluateChain();
  }
  if (status == TerminationPredicate::Status::PROCEED) {
    return evaluate();
  }
  return status;
}

TerminationPredicate::Status DurationTerminationPredicateImpl::evaluate() {
  return time_source_.monotonicTime() - start_ > duration_ ? TerminationPredicate::Status::TERMINATE
                                                           : TerminationPredicate::Status::PROCEED;
}

TerminationPredicate::Status StatsCounterAbsoluteThresholdTerminationPredicateImpl::evaluate() {
  return counter_.value() > counter_limit_ ? termination_status_
                                           : TerminationPredicate::Status::PROCEED;
}

} // namespace Nighthawk