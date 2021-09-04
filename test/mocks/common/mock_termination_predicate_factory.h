#pragma once

#include "nighthawk/common/factories.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockTerminationPredicateFactory : public TerminationPredicateFactory {
public:
  MockTerminationPredicateFactory();
  MOCK_METHOD(TerminationPredicatePtr, create,
              (Envoy::TimeSource & time_source, Envoy::Stats::Scope& scope,
               const Envoy::MonotonicTime scheduled_starting_time),
              (const, override));
};

} // namespace Nighthawk