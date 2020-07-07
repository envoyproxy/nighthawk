#pragma once

#include "api/adaptive_rps/metrics_plugin.pb.h"
#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

namespace Nighthawk {
namespace AdaptiveRps {

// An interface for plugins that retrieve platform-specific metrics from data sources outside
// Nighthawk. Connection info is passed via a plugin-specific config proto.
//
// To implement a MetricsPlugin:
//   
class MetricsPlugin {
public:
  virtual ~MetricsPlugin() = default;
  // Obtain the numeric metric with the given name, usually by querying an outside system.
  virtual double GetMetricByName(const std::string& metric_name) PURE;
  // All metric names implemented by this plugin, for use in input validation.
  virtual std::vector<std::string> GetAllSupportedMetricNames() PURE;
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
