#pragma once

#include <chrono>
#include <memory>

#include "envoy/api/api.h"
#include "envoy/common/time.h"
#include "envoy/event/dispatcher.h"
#include "envoy/stats/store.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/client/options.h"
#include "nighthawk/common/platform_util.h"
#include "nighthawk/common/rate_limiter.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/uri.h"

#include "common/utility.h"

#include "test/test_common/simulated_time_system.h"

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
};

class MockSequencer : public Sequencer {
public:
  MockSequencer();

  MOCK_METHOD0(start, void());
  MOCK_METHOD0(waitForCompletion, void());
  MOCK_CONST_METHOD0(completionsPerSecond, double());
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
  MOCK_CONST_METHOD0(uri, std::string());
  MOCK_CONST_METHOD0(h2, bool());
  MOCK_CONST_METHOD0(concurrency, std::string());
  MOCK_CONST_METHOD0(verbosity, std::string());
  MOCK_CONST_METHOD0(outputFormat, std::string());
  MOCK_CONST_METHOD0(prefetchConnections, bool());
  MOCK_CONST_METHOD0(burstSize, uint32_t());
  MOCK_CONST_METHOD0(addressFamily, nighthawk::client::AddressFamily::AddressFamilyOptions());
  MOCK_CONST_METHOD0(requestMethod, std::string());
  MOCK_CONST_METHOD0(requestHeaders, std::vector<std::string>());
  MOCK_CONST_METHOD0(requestBodySize, uint32_t());
  MOCK_CONST_METHOD0(tlsContext, envoy::api::v2::auth::UpstreamTlsContext&());
  MOCK_CONST_METHOD0(maxPendingRequests, uint32_t());
  MOCK_CONST_METHOD0(maxActiveRequests, uint32_t());
  MOCK_CONST_METHOD0(maxRequestsPerConnection, uint32_t());
  MOCK_CONST_METHOD0(toCommandLineOptions, Client::CommandLineOptionsPtr());
  MOCK_CONST_METHOD0(sequencerIdleStrategy, std::string());
};

class MockBenchmarkClientFactory : public Client::BenchmarkClientFactory {
public:
  MockBenchmarkClientFactory();
  MOCK_CONST_METHOD4(create, Client::BenchmarkClientPtr(Envoy::Api::Api& api,
                                                        Envoy::Event::Dispatcher& dispatcher,
                                                        Envoy::Stats::Store& store, UriPtr&& uri));
};

class MockSequencerFactory : public Client::SequencerFactory {
public:
  MockSequencerFactory();
  MOCK_CONST_METHOD4(create, SequencerPtr(Envoy::TimeSource& time_source,
                                          Envoy::Event::Dispatcher& dispatcher,
                                          Envoy::MonotonicTime start_time,
                                          Client::BenchmarkClient& benchmark_client));
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

class FakeSequencerTarget {
public:
  virtual ~FakeSequencerTarget() = default;
  // A fake method that matches the sequencer target signature.
  virtual bool callback(std::function<void()>) PURE;
};

class MockSequencerTarget : public FakeSequencerTarget {
public:
  MockSequencerTarget();
  MOCK_METHOD1(callback, bool(std::function<void()>));
};

class MockBenchmarkClient : public Client::BenchmarkClient {
public:
  MockBenchmarkClient();

  MOCK_METHOD1(initialize, void(Envoy::Runtime::Loader&));
  MOCK_METHOD0(terminate, void());
  MOCK_METHOD1(setMeasureLatencies, void(bool));
  MOCK_CONST_METHOD0(statistics, StatisticPtrMap());
  MOCK_METHOD1(tryStartOne, bool(std::function<void()>));
  MOCK_CONST_METHOD0(store, Envoy::Stats::Store&());
  MOCK_METHOD0(prefetchPoolConnections, void());

  MOCK_METHOD1(setRequestMethod, void(absl::string_view));
  MOCK_METHOD2(setRequestHeader, void(absl::string_view, absl::string_view));
  MOCK_METHOD1(setRequestBodySize, void(uint32_t));
  MOCK_CONST_METHOD0(requestHeaders, const Envoy::Http::HeaderMap&());

protected:
  MOCK_CONST_METHOD0(measureLatencies, bool());
};

} // namespace Nighthawk
