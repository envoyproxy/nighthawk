#pragma once

#include <chrono>
#include <memory>

#include "envoy/api/api.h"
#include "envoy/common/time.h"
#include "envoy/event/dispatcher.h"
#include "envoy/stats/store.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/client/options.h"
#include "nighthawk/common/platform_util.h"
#include "nighthawk/common/rate_limiter.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/termination_predicate.h"
#include "nighthawk/common/uri.h"

#include "external/envoy/test/test_common/simulated_time_system.h"

#include "common/utility.h"

#include "absl/types/optional.h"
#include "gmock/gmock.h"

using namespace std::chrono_literals;

constexpr std::chrono::milliseconds TimeResolution = 1ms;

namespace Nighthawk {

// TODO(oschaaf): split this out in files for common/ and client/ mocks

class MockPlatformUtil : public PlatformUtil {
public:
  MockPlatformUtil();

  MOCK_CONST_METHOD0(yieldCurrentThread, void());
  MOCK_CONST_METHOD1(sleep, void(std::chrono::microseconds));
};

class MockRateLimiter : public RateLimiter {
public:
  MockRateLimiter();

  MOCK_METHOD0(tryAcquireOne, bool());
  MOCK_METHOD0(releaseOne, void());
  MOCK_METHOD0(timeSource, Envoy::TimeSource&());
  MOCK_METHOD0(elapsed, std::chrono::nanoseconds());
};

class MockSequencer : public Sequencer {
public:
  MockSequencer();

  MOCK_METHOD0(start, void());
  MOCK_METHOD0(waitForCompletion, void());
  MOCK_CONST_METHOD0(completionsPerSecond, double());
  MOCK_CONST_METHOD0(executionDuration, std::chrono::nanoseconds());
  MOCK_CONST_METHOD0(statistics, StatisticPtrMap());
  MOCK_METHOD0(cancel, void());
};

class MockOptions : public Client::Options {
public:
  MockOptions();

  MOCK_CONST_METHOD0(requestsPerSecond, uint32_t());
  MOCK_CONST_METHOD0(connections, uint32_t());
  MOCK_CONST_METHOD0(duration, std::chrono::seconds());
  MOCK_CONST_METHOD0(timeout, std::chrono::seconds());
  MOCK_CONST_METHOD0(uri, absl::optional<std::string>());
  MOCK_CONST_METHOD0(h2, bool());
  MOCK_CONST_METHOD0(concurrency, std::string());
  MOCK_CONST_METHOD0(verbosity, nighthawk::client::Verbosity::VerbosityOptions());
  MOCK_CONST_METHOD0(outputFormat, nighthawk::client::OutputFormat::OutputFormatOptions());
  MOCK_CONST_METHOD0(prefetchConnections, bool());
  MOCK_CONST_METHOD0(burstSize, uint32_t());
  MOCK_CONST_METHOD0(addressFamily, nighthawk::client::AddressFamily::AddressFamilyOptions());
  MOCK_CONST_METHOD0(requestMethod, envoy::config::core::v3alpha::RequestMethod());
  MOCK_CONST_METHOD0(requestHeaders, std::vector<std::string>());
  MOCK_CONST_METHOD0(requestBodySize, uint32_t());
  MOCK_CONST_METHOD0(tlsContext,
                     envoy::extensions::transport_sockets::tls::v3alpha::UpstreamTlsContext&());
  MOCK_CONST_METHOD0(transportSocket,
                     absl::optional<envoy::config::core::v3alpha::TransportSocket>&());
  MOCK_CONST_METHOD0(maxPendingRequests, uint32_t());
  MOCK_CONST_METHOD0(maxActiveRequests, uint32_t());
  MOCK_CONST_METHOD0(maxRequestsPerConnection, uint32_t());
  MOCK_CONST_METHOD0(toCommandLineOptions, Client::CommandLineOptionsPtr());
  MOCK_CONST_METHOD0(sequencerIdleStrategy,
                     nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions());
  MOCK_CONST_METHOD0(requestSource, std::string());
  MOCK_CONST_METHOD0(trace, std::string());
  MOCK_CONST_METHOD0(
      h1ConnectionReuseStrategy,
      nighthawk::client::H1ConnectionReuseStrategy::H1ConnectionReuseStrategyOptions());
  MOCK_CONST_METHOD0(terminationPredicates, Client::TerminationPredicateMap());
  MOCK_CONST_METHOD0(failurePredicates, Client::TerminationPredicateMap());
  MOCK_CONST_METHOD0(openLoop, bool());
  MOCK_CONST_METHOD0(jitterUniform, std::chrono::nanoseconds());
  MOCK_CONST_METHOD0(nighthawkService, std::string());
  MOCK_CONST_METHOD0(h2UseMultipleConnections, bool());
  MOCK_CONST_METHOD0(multiTargetEndpoints, std::vector<nighthawk::client::MultiTarget::Endpoint>());
  MOCK_CONST_METHOD0(multiTargetPath, std::string());
  MOCK_CONST_METHOD0(multiTargetUseHttps, bool());
  MOCK_CONST_METHOD0(labels, std::vector<std::string>());
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

class FakeSequencerTarget {
public:
  virtual ~FakeSequencerTarget() = default;
  // A fake method that matches the sequencer target signature.
  virtual bool callback(OperationCallback) PURE;
};

class MockSequencerTarget : public FakeSequencerTarget {
public:
  MockSequencerTarget();
  MOCK_METHOD1(callback, bool(OperationCallback));
};

class MockBenchmarkClient : public Client::BenchmarkClient {
public:
  MockBenchmarkClient();

  MOCK_METHOD0(terminate, void());
  MOCK_METHOD1(setShouldMeasureLatencies, void(bool));
  MOCK_CONST_METHOD0(statistics, StatisticPtrMap());
  MOCK_METHOD1(tryStartRequest, bool(Client::CompletionCallback));
  MOCK_CONST_METHOD0(scope, Envoy::Stats::Scope&());
  MOCK_CONST_METHOD0(shouldMeasureLatencies, bool());
  MOCK_CONST_METHOD0(requestHeaders, const Envoy::Http::HeaderMap&());
};

class MockRequestSource : public RequestSource {
public:
  MockRequestSource();
  MOCK_METHOD0(get, RequestGenerator());
  MOCK_METHOD0(initOnThread, void());
};

class MockTerminationPredicate : public TerminationPredicate {
public:
  MockTerminationPredicate();
  MOCK_METHOD1(link, TerminationPredicate&(TerminationPredicatePtr&&));
  MOCK_METHOD1(appendToChain, TerminationPredicate&(TerminationPredicatePtr&&));
  MOCK_METHOD0(evaluateChain, TerminationPredicate::Status());
  MOCK_METHOD0(evaluate, TerminationPredicate::Status());
};

class MockDiscreteNumericDistributionSampler : public DiscreteNumericDistributionSampler {
public:
  MockDiscreteNumericDistributionSampler();
  MOCK_METHOD0(getValue, uint64_t());
  MOCK_CONST_METHOD0(min, uint64_t());
  MOCK_CONST_METHOD0(max, uint64_t());
};

} // namespace Nighthawk
