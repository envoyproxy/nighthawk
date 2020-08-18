#include "test/adaptive_load/fake_plugins/fake_metrics_plugin/fake_metrics_plugin.h"
#include "api/adaptive_load/benchmark_result.pb.h"

namespace Nighthawk {

FakeMetricsPlugin::FakeMetricsPlugin(
    const nighthawk::adaptive_load::FakeMetricsPluginConfig& config)
    : fixed_metric_value_{config.fixed_metric_value()} {}

absl::StatusOr<double> FakeMetricsPlugin::GetMetricByName(absl::string_view metric_name) {
  if (metric_name == "bad-metric") {
    return absl::InternalError("bad-metric requested");
  }
  return fixed_metric_value_;
}

const std::vector<std::string> FakeMetricsPlugin::GetAllSupportedMetricNames() const {
  return {"good-metric", "bad-metric"};
}

std::string FakeMetricsPluginConfigFactory::name() const { return "nighthawk.fake-metrics-plugin"; }

Envoy::ProtobufTypes::MessagePtr FakeMetricsPluginConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_load::FakeMetricsPluginConfig>();
}

MetricsPluginPtr
FakeMetricsPluginConfigFactory::createMetricsPlugin(const Envoy::Protobuf::Message& message) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_load::FakeMetricsPluginConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<FakeMetricsPlugin>(config);
}

REGISTER_FACTORY(FakeMetricsPluginConfigFactory, MetricsPluginConfigFactory);

envoy::config::core::v3::TypedExtensionConfig
MakeFakeMetricsPluginConfig(double fixed_metric_value) {
  envoy::config::core::v3::TypedExtensionConfig outer_config;
  outer_config.set_name("nighthawk.fake-metrics-plugin");
  nighthawk::adaptive_load::FakeMetricsPluginConfig config;
  config.set_fixed_metric_value(fixed_metric_value);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  *outer_config.mutable_typed_config() = config_any;
  return outer_config;
}

} // namespace Nighthawk