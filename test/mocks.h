#pragma once

#include "envoy/api/api.h"
#include "envoy/common/time.h"
#include "envoy/event/dispatcher.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/client/factories.h"
#include "nighthawk/common/statistic.h"

#include "gmock/gmock.h"

namespace Nighthawk {

// TODO(oschaaf): split this out in files for common/ and client/ mocks

class MockSequencerFactory : public Client::SequencerFactory {
public:
  MockSequencerFactory();
  MOCK_CONST_METHOD6(create, SequencerPtr(Envoy::TimeSource& time_source,
                                          Envoy::Event::Dispatcher& dispatcher,
                                          Client::BenchmarkClient& benchmark_client,
                                          TerminationPredicatePtr&& termination_predicate,
                                          Envoy::Stats::Scope& scope,
                                          const Envoy::MonotonicTime scheduled_starting_time));
};

class MockStatisticFactory : public Client::StatisticFactory {
public:
  MockStatisticFactory();
  MOCK_CONST_METHOD0(create, StatisticPtr());
};

class MockRequestSourceFactory : public RequestSourceFactory {
public:
  MockRequestSourceFactory();
  MOCK_CONST_METHOD4(create,
                     RequestSourcePtr(const Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                                      Envoy::Event::Dispatcher& dispatcher,
                                      Envoy::Stats::Scope& scope,
                                      absl::string_view service_cluster_name));
};

class MockTerminationPredicateFactory : public TerminationPredicateFactory {
public:
  MockTerminationPredicateFactory();
  MOCK_CONST_METHOD3(create,
                     TerminationPredicatePtr(Envoy::TimeSource& time_source,
                                             Envoy::Stats::Scope& scope,
                                             const Envoy::MonotonicTime scheduled_starting_time));
};

} // namespace Nighthawk
