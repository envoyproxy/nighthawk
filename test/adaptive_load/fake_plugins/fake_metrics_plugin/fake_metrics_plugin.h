#pragma once

#include "envoy/registry/registry.h"

#include "nighthawk/adaptive_load/metrics_plugin.h"

#include "api/client/options.pb.h"

#include "test/adaptive_load/fake_plugins/fake_metrics_plugin/fake_metrics_plugin.pb.h"

namespace Nighthawk {

/**
 * MetricsPlugin for testing, supporting fixed values and artificial errors.
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
   * @return StatusOr<double> The fixed metric value or error status configured at the given metric
   * name.
   */
  absl::StatusOr<double> GetMetricByName(absl::string_view metric_name) override;
  /**
   * @return std::vector<std::string> Names of all fake metrics configured via the config proto.
   */
  const std::vector<std::string> GetAllSupportedMetricNames() const override;

private:
  absl::flat_hash_map<std::string, absl::StatusOr<double>> value_or_error_from_name_;
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
 * Creates a TypedExtensionConfig that activates a FakeMetricsPlugin by name with the given config
 * proto.
 *
 * @param config The plugin-specific config proto to be packed into the typed_config Any.
 *
 * @return TypedExtensionConfig A proto that activates a FakeMetricsPlugin by name with a bundled
 * config proto.
 */
envoy::config::core::v3::TypedExtensionConfig MakeFakeMetricsPluginTypedExtensionConfig(
    const nighthawk::adaptive_load::FakeMetricsPluginConfig& config);

} // namespace Nighthawk
