// Interfaces for MetricsPlugin plugins and plugin factories.

#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "nighthawk/adaptive_load/config_validator.h"

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
   * Obtains the numeric metric with the given name, usually by querying an outside system. Provides
   * start_time and duration of measurement period for the plugin to utilitize.
   *
   * @param metric_name The name of the metric to retrieve. Must be supported by the plugin.
   * @param start_time start_time with duration indicates when the metric is relevant.
   * @param duration start_time with duration indicates when the metric is relevant.
   *
   * @return StatusOr<double> The metric value, or an error status if the metric was unsupported or
   * unavailable.
   */
  virtual absl::StatusOr<double>
  GetMetricByNameWithTime(absl::string_view metric_name,
                          const google::protobuf::Timestamp& start_time,
                          const google::protobuf::Duration& duration);

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
class MetricsPluginConfigFactory : public Envoy::Config::TypedFactory, public ConfigValidator {
public:
  std::string category() const override { return "nighthawk.metrics_plugin"; }
  /**
   * Instantiates the specific MetricsPlugin class. Casts |message| to Any, unpacks it to the
   * plugin-specific proto, and passes the strongly typed proto to the plugin constructor.
   *
   * @param message Any typed_config proto taken from the TypedExtensionConfig.
   *
   * @return MetricsPluginPtr Pointer to the new plugin instance.
   *
   * @throw Envoy::EnvoyException If the Any proto cannot be unpacked as the type expected by the
   * plugin.
   */
  virtual MetricsPluginPtr createMetricsPlugin(const Envoy::Protobuf::Message& message) PURE;
};

} // namespace Nighthawk
