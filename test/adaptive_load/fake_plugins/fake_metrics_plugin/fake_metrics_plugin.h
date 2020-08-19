#pragma once

#include "api/client/options.pb.h"
#include "envoy/registry/registry.h"
#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "test/adaptive_load/fake_plugins/fake_metrics_plugin/fake_metrics_plugin.pb.h"

namespace Nighthawk {

/**
 * MetricsPlugin for testing: Configurable fixed metric value.
 */
class FakeMetricsPlugin : public MetricsPlugin {
public:
  /**
   * Initializes the fake plugin with a FakeMetricsPluginConfig proto.
   *
   * @param config FakeMetricsPluginConfig proto for setting the fixed metric value.
   */
  FakeMetricsPlugin(const nighthawk::adaptive_load::FakeMetricsPluginConfig& config);
  /**
   * @return StatusOr<double> The fixed metric value if the metric name was "good-metric", error
   * status if the metric name was "bad-metric".
   */
  absl::StatusOr<double> GetMetricByName(absl::string_view metric_name) override;
  /**
   * @return std::vector<std::string> {"good-metric", "bad-metric"}.
   */
  const std::vector<std::string> GetAllSupportedMetricNames() const override;

private:
  const double fixed_metric_value_;
};

/**
 * Factory that creates a FakeMetricsPlugin plugin from a FakeMetricsPluginConfig proto.
 * Registered as an Envoy plugin.
 */
class FakeMetricsPluginConfigFactory : public MetricsPluginConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  MetricsPluginPtr createMetricsPlugin(const Envoy::Protobuf::Message& config_any) override;
  absl::Status ValidateConfig(const Envoy::Protobuf::Message& message) const override;
};

// This factory is activated through LoadMetricsPlugin in plugin_util.h.
DECLARE_FACTORY(FakeMetricsPluginConfigFactory);

/**
 * Creates a valid TypedExtensionConfig proto that activates a FakeMetricsPlugin with a
 * FakeMetricsPluginConfig.
 *
 * @param fixed_metric_value A value that the plugin should always return when "good-metric" is
 * requested.
 *
 * @return TypedExtensionConfig A proto that activates FakeMetricsPlugin by name and includes a
 * FakeMetricsPluginConfig proto wrapped in an Any.
 */
envoy::config::core::v3::TypedExtensionConfig MakeFakeMetricsPluginConfig(double fixed_metric_value);

} // namespace Nighthawk
