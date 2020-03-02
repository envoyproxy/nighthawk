#pragma once

#include "nighthawk/common/factories.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockTerminationPredicateFactory : public TerminationPredicateFactory {
public:
  MockTerminationPredicateFactory();
  MOCK_CONST_METHOD3(create,
                     TerminationPredicatePtr(Envoy::TimeSource& time_source,
                                             Envoy::Stats::Scope& scope,
                                             const Envoy::MonotonicTime scheduled_starting_time));
};

} // namespace Nighthawk