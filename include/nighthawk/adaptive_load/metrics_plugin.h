// Interfaces for MetricsPlugin plugins and plugin factories.

#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/core/v3/base.pb.h"
#include "envoy/config/typed_config.h"

#include "external/envoy/source/common/common/statusor.h"

namespace Nighthawk {

/**
 * An interface for plugins that retrieve platform-specific metrics from outside data sources.
 * Connection info is passed via a plugin-specific config proto.
 */
class MetricsPlugin {
public:
  virtual ~MetricsPlugin() = default;
  /**
   * Obtains the numeric metric with the given name, usually by querying an outside system.
   *
   * @param metric_name The name of the metric to retrieve. Must be supported by the plugin.
   *
   * @return StatusOr<double> The metric value, or an error status if the metric was unsupported or
   * unavailable.
   */
  virtual absl::StatusOr<double> GetMetricByName(absl::string_view metric_name) PURE;
  /**
   * All metric names implemented by this plugin, for use in input validation.
   *
   * @return const std::vector<std::string> List of metric names that can be queried from this
   * plugin.
   */
  virtual const std::vector<std::string> GetAllSupportedMetricNames() const PURE;
};

using MetricsPluginPtr = std::unique_ptr<MetricsPlugin>;

/**
 * A factory that must be implemented for each MetricsPlugin. It instantiates the specific
 * MetricsPlugin class after unpacking the plugin-specific config proto.
 */
class MetricsPluginConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~MetricsPluginConfigFactory() override = default;
  std::string category() const override { return "nighthawk.metrics_plugin"; }
  /**
   * Instantiates the specific MetricsPlugin class. Casts |message| to Any, unpacks it to the
   * plugin-specific proto, and passes the strongly typed proto to the plugin constructor.
   *
   * @param message Any typed_config proto taken from the TypedExtensionConfig.
   *
   * @return MetricsPluginPtr Pointer to the new plugin instance.
   */
  virtual MetricsPluginPtr createMetricsPlugin(const Envoy::Protobuf::Message& message) PURE;
};

} // namespace Nighthawk
