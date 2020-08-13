#include "test/adaptive_load/fake_plugins/fake_metrics_plugin/fake_metrics_plugin_impl.h"
#include "api/adaptive_load/benchmark_result.pb.h"

namespace Nighthawk {

FakeMetricsPlugin::FakeMetricsPlugin(
    const nighthawk::adaptive_load::FakeMetricsPluginConfig& config)
    : fixed_metric_value_{config.fixed_metric_value()} {}

absl::StatusOr<double>
FakeMetricsPlugin::GetMetricByName(absl::string_view metric_name) {
  if (metric_name == "bad-metric") {
    return absl::InternalError("bad-metric requested");
  }
  return fixed_metric_value_;
}

const std::vector<std::string> FakeMetricsPlugin::GetAllSupportedMetricNames() const {
  return {"good-metric", "bad-metric"};
}

std::string FakeMetricsPluginConfigFactory::name() const {
  return "nighthawk.fake-metrics-plugin";
}

Envoy::ProtobufTypes::MessagePtr FakeMetricsPluginConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_load::FakeMetricsPluginConfig>();
}

MetricsPluginPtr FakeMetricsPluginConfigFactory::createMetricsPlugin(
    const Envoy::Protobuf::Message& message) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_load::FakeMetricsPluginConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<FakeMetricsPlugin>(config);
}

REGISTER_FACTORY(FakeMetricsPluginConfigFactory, MetricsPluginConfigFactory);

} // namespace Nighthawk