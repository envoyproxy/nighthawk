// Interfaces for MetricsPlugin plugins and plugin factories.

#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "nighthawk/adaptive_load/config_validator.h"

#include "external/envoy/source/common/common/statusor.h"

namespace Nighthawk {

// Describes the period of time where the Nighthawk test iteration is sending the intended load.
// Metric Plugins should report metrics relevant to this time period.
// For example, if a plugin is tracking the peak memory usage of a system under test. When given
// this data, it should filter the memory usage samples to only include data points in this time
// period and then calculate the peak usage out of those data points.
struct ReportingPeriod {
  // start time of the latest (current) iteration of Nighthawk test in the adaptive stage. See
  // https://github.com/envoyproxy/nighthawk/blob/main/docs/root/adaptive_load_controller.md#the-adaptive-load-controller
  // for more information on adaptive load testing.
  google::protobuf::Timestamp start_time;

  // The duration of the time where nighthawk is sending the intended load in the adaptive stage.
  google::protobuf::Duration duration;
};

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
  ABSL_DEPRECATED("Use GetMetricByNameWithReportingPeriod instead.")
  virtual absl::StatusOr<double> GetMetricByName(absl::string_view metric_name) PURE;

  /**
   * Obtains the numeric metric with the given name, usually by querying an outside system. Provides
   * reporting_period to allow plugins to determine what metrics to consider report.
   * For example, if a plugin is tracking the peak memory usage of a system under test. When given
   * this data, it should filter the memory usage samples to only include data points in this time
   * period and then calculate the peak usage out of those data points.
   *
   * @param metric_name The name of the metric to retrieve. Must be supported by the plugin.
   * @param reporting_period the time period the Nighthawk test iteration is sending the intended
   * load (i.e. the time period in which the metrics are of interest).
   *
   * @return StatusOr<double> The metric value, or an error status if the metric was unsupported or
   * unavailable.
   */
  virtual absl::StatusOr<double>
  GetMetricByNameWithReportingPeriod(absl::string_view metric_name,
                                     const ReportingPeriod& reporting_period);

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
