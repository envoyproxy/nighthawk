#pragma once

#include <chrono>
#include <memory>

#include "envoy/api/api.h"
#include "envoy/common/time.h"
#include "envoy/event/dispatcher.h"
#include "envoy/stats/store.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/client/factories.h"
#include "nighthawk/common/platform_util.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/test/test_common/simulated_time_system.h"

#include "common/utility.h"

#include "absl/types/optional.h"
#include "gmock/gmock.h"

using namespace std::chrono_literals;

namespace Nighthawk {

// TODO(oschaaf): split this out in files for common/ and client/ mocks

class MockPlatformUtil : public PlatformUtil {
public:
  MockPlatformUtil();

  MOCK_CONST_METHOD0(yieldCurrentThread, void());
  MOCK_CONST_METHOD1(sleep, void(std::chrono::microseconds));
};

class MockBenchmarkClientFactory : public Client::BenchmarkClientFactory {
public:
  MockBenchmarkClientFactory();
  MOCK_CONST_METHOD7(create,
                     Client::BenchmarkClientPtr(Envoy::Api::Api&, Envoy::Event::Dispatcher&,
                                                Envoy::Stats::Scope&,
                                                Envoy::Upstream::ClusterManagerPtr&,
                                                Envoy::Tracing::HttpTracerPtr&, absl::string_view,
                                                RequestSource& request_generator));
};

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

class MockStoreFactory : public Client::StoreFactory {
public:
  MockStoreFactory();
  MOCK_CONST_METHOD0(create, Envoy::Stats::StorePtr());
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

class MockRequestSource : public RequestSource {
public:
  MockRequestSource();
  MOCK_METHOD0(get, RequestGenerator());
  MOCK_METHOD0(initOnThread, void());
};

} // namespace Nighthawk
