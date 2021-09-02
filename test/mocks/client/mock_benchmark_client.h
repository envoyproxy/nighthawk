#pragma once

#include "nighthawk/client/benchmark_client.h"

#include "gmock/gmock.h"

namespace Nighthawk {
namespace Client {

class MockBenchmarkClient : public BenchmarkClient {
public:
  MockBenchmarkClient();

  MOCK_METHOD(void, terminate, (), (override));
  MOCK_METHOD(void, setShouldMeasureLatencies, (bool), (override));
  MOCK_METHOD(StatisticPtrMap, statistics, (), (const, override));
  MOCK_METHOD(bool, tryStartRequest, (Client::CompletionCallback), (override));
  MOCK_METHOD(Envoy::Stats::Scope&, scope, (), (const, override));
  MOCK_METHOD(bool, shouldMeasureLatencies, (), (const, override));
  MOCK_METHOD(const Envoy::Http::RequestHeaderMap&, requestHeaders, (),
              (const));
};

} // namespace Client
}  // namespace Nighthawk
