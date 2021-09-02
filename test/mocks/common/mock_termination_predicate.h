#pragma once

#include "nighthawk/common/termination_predicate.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockTerminationPredicate : public TerminationPredicate {
public:
  MockTerminationPredicate();
  MOCK_METHOD(TerminationPredicate&, link, (TerminationPredicatePtr &&), (override));
  MOCK_METHOD(TerminationPredicate&, appendToChain, (TerminationPredicatePtr &&), (override));
  MOCK_METHOD(TerminationPredicate::Status, evaluateChain, (), (override));
  MOCK_METHOD(TerminationPredicate::Status, evaluate, (), (override));
};

} // namespace Nighthawk