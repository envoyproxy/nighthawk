#pragma once

#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/client/factories.h"

#include "gmock/gmock.h"

namespace Nighthawk {
namespace Client {

class MockBenchmarkClientFactory : public BenchmarkClientFactory {
public:
  MockBenchmarkClientFactory();
  MOCK_CONST_METHOD7(create,
                     BenchmarkClientPtr(Envoy::Api::Api&, Envoy::Event::Dispatcher&,
                                        Envoy::Stats::Scope&, Envoy::Upstream::ClusterManagerPtr&,
                                        Envoy::Tracing::HttpTracerSharedPtr&, absl::string_view,
                                        const int, RequestSource& request_generator));
};

} // namespace Client
} // namespace Nighthawk
