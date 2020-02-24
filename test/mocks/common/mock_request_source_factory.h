#pragma once

#include "nighthawk/common/factories.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockRequestSourceFactory : public RequestSourceFactory {
public:
  MockRequestSourceFactory();
  MOCK_CONST_METHOD4(create,
                     RequestSourcePtr(const Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                                      Envoy::Event::Dispatcher& dispatcher,
                                      Envoy::Stats::Scope& scope,
                                      absl::string_view service_cluster_name));
};

} // namespace Nighthawk