#pragma once

#include "nighthawk/common/termination_predicate.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockTerminationPredicate : public TerminationPredicate {
public:
  MockTerminationPredicate();
  MOCK_METHOD1(link, TerminationPredicate&(TerminationPredicatePtr&&));
  MOCK_METHOD1(appendToChain, TerminationPredicate&(TerminationPredicatePtr&&));
  MOCK_METHOD0(evaluateChain, TerminationPredicate::Status());
  MOCK_METHOD0(evaluate, TerminationPredicate::Status());
};

} // namespace Nighthawk