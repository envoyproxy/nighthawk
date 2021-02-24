#pragma once

#include "nighthawk/client/benchmark_client.h"

#include "gmock/gmock.h"

namespace Nighthawk {
namespace Client {

class MockBenchmarkClient : public BenchmarkClient {
public:
  MockBenchmarkClient();

  MOCK_METHOD(void, terminate, ());
  MOCK_METHOD(void, setShouldMeasureLatencies, (bool));
  MOCK_METHOD(StatisticPtrMap, statistics, (), (const));
  MOCK_METHOD(bool, tryStartRequest, (Client::CompletionCallback));
  MOCK_METHOD(Envoy::Stats::Scope&, scope, (), (const));
  MOCK_METHOD(bool, shouldMeasureLatencies, (), (const));
  MOCK_METHOD(const Envoy::Http::RequestHeaderMap&, requestHeaders, (), (const));
};

} // namespace Client
} // namespace Nighthawk