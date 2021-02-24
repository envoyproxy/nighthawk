#pragma once

#include "nighthawk/common/termination_predicate.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockTerminationPredicate : public TerminationPredicate {
public:
  MockTerminationPredicate();
  MOCK_METHOD(TerminationPredicate&, link, (TerminationPredicatePtr && p));
  MOCK_METHOD(TerminationPredicate&, appendToChain, (TerminationPredicatePtr && p));
  MOCK_METHOD(TerminationPredicate::Status, evaluateChain, ());
  MOCK_METHOD(TerminationPredicate::Status, evaluate, ());
};

} // namespace Nighthawk