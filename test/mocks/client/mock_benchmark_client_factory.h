#pragma once

#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/client/factories.h"
#include "nighthawk/user_defined_output/user_defined_output_plugin.h"

#include "gmock/gmock.h"

namespace Nighthawk {
namespace Client {

class MockBenchmarkClientFactory : public BenchmarkClientFactory {
public:
  MockBenchmarkClientFactory();
  MOCK_METHOD(BenchmarkClientPtr, create,
              (Envoy::Api::Api&, Envoy::Event::Dispatcher&, Envoy::Stats::Scope&,
               Envoy::Upstream::ClusterManagerPtr&, Envoy::Tracing::HttpTracerSharedPtr&,
               absl::string_view, int, RequestSource& request_generator,
               std::vector<UserDefinedOutputNamePluginPair> user_defined_output_plugins),
              (const, override));
};

} // namespace Client
} // namespace Nighthawk
