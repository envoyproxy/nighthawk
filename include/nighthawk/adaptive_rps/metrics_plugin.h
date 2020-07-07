#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "api/adaptive_rps/metrics_plugin.pb.h"

namespace Nighthawk {
namespace AdaptiveRps {

// An interface for plugins that retrieve platform-specific metrics from outside data sources.
// Connection info is passed via a plugin-specific config proto.
//
// See source/adaptive_rps/metrics_plugin_impl.h for an example plugin.
class MetricsPlugin {
public:
  virtual ~MetricsPlugin() = default;
  // Obtain the numeric metric with the given name, usually by querying an outside system.
  virtual double GetMetricByName(const std::string& metric_name) PURE;
  // All metric names implemented by this plugin, for use in input validation.
  virtual std::vector<std::string> GetAllSupportedMetricNames() PURE;
};

using MetricsPluginPtr = std::unique_ptr<MetricsPlugin>;

// A factory that must be implemented for each MetricsPlugin. It instantiates the specific
// MetricsPlugin class after unpacking the plugin-specific config proto.
class MetricsPluginConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~MetricsPluginConfigFactory() override = default;
  std::string category() const override { return "nighthawk.metrics_plugin"; }
  // Instantiates the specific MetricsPlugin class. Casts |message| to Any, unpacks it to the
  // plugin-specific proto, and passes the strongly typed proto to the constructor.
  virtual MetricsPluginPtr createMetricsPlugin(const Envoy::Protobuf::Message&) PURE;
};

} // namespace AdaptiveRps
} // namespace Nighthawk
