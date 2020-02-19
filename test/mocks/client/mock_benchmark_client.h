#pragma once

#include "nighthawk/client/benchmark_client.h"

#include "gmock/gmock.h"

namespace Nighthawk {
namespace Client {

class MockBenchmarkClient : public BenchmarkClient {
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

} // namespace Client
} // namespace Nighthawk