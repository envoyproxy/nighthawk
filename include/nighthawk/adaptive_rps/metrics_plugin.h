#pragma once

#include "api/adaptive_rps/metrics_plugin.pb.h"
#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"
// #include "envoy/common/config/utility.h"
// #include "envoy/common/protobuf/protobuf.h"

namespace Nighthawk {
namespace AdaptiveRps {

class MetricsPlugin {
public:
  virtual ~MetricsPlugin() = default;
  virtual double GetMetricByName(const std::string& metric_name) PURE;
};

using MetricsPluginPtr = std::unique_ptr<MetricsPlugin>;

class MetricsPluginConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~MetricsPluginConfigFactory() override = default;
  std::string category() const override { return "nighthawk.metrics_plugin"; }
  virtual MetricsPluginPtr createMetricsPlugin(const Envoy::Protobuf::Message&) PURE;
};

} // namespace AdaptiveRps
} // namespace Nighthawk
