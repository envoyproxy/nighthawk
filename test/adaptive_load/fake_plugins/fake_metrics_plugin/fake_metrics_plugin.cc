#include "test/adaptive_load/fake_plugins/fake_metrics_plugin/fake_metrics_plugin.h"

#include "envoy/common/exception.h"

#include "api/adaptive_load/benchmark_result.pb.h"

#include "test/adaptive_load/fake_plugins/fake_metrics_plugin/fake_metrics_plugin.pb.h"

namespace Nighthawk {

namespace {

absl::Status GetStatusFromProtoRpcStatus(const google::rpc::Status& status_proto) {
  return absl::Status{static_cast<absl::StatusCode>(status_proto.code()), status_proto.message()};
}

absl::StatusOr<double> StatusOrFromFakeMetricProto(
    const nighthawk::adaptive_load::FakeMetricsPluginConfig::FakeMetric& fake_metric) {
  if (fake_metric.has_error_status()) {
    return GetStatusFromProtoRpcStatus(fake_metric.error_status());
  } else {
    return fake_metric.value();
  }
}

} // namespace

FakeMetricsPlugin::FakeMetricsPlugin(
    const nighthawk::adaptive_load::FakeMetricsPluginConfig& config) {
  for (const nighthawk::adaptive_load::FakeMetricsPluginConfig::FakeMetric& fake_metric :
       config.fake_metrics()) {
    value_or_error_from_name_[fake_metric.name()] = StatusOrFromFakeMetricProto(fake_metric);
  }
}

absl::StatusOr<double> FakeMetricsPlugin::GetMetricByName(absl::string_view metric_name) {
  if (value_or_error_from_name_.find(metric_name) == value_or_error_from_name_.end()) {
    return absl::InternalError(absl::StrCat("GetMetricByName called on metric name '", metric_name,
                                            "' not defined in FakeMetricsPluginConfig proto."));
  }
  return value_or_error_from_name_[metric_name];
}


const std::vector<std::string> FakeMetricsPlugin::GetAllSupportedMetricNames() const {
  std::vector<std::string> metric_names;
  for (const auto& key_value : value_or_error_from_name_) {
    metric_names.push_back(key_value.first);
  }
  return metric_names;
}

std::string FakeMetricsPluginConfigFactory::name() const { return "nighthawk.fake_metrics_plugin"; }

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

absl::Status
FakeMetricsPluginConfigFactory::ValidateConfig(const Envoy::Protobuf::Message& message) const {
  try {
    const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
    nighthawk::adaptive_load::FakeMetricsPluginConfig config;
    Envoy::MessageUtil::unpackTo(any, config);
    if (config.has_artificial_validation_failure()) {
      return GetStatusFromProtoRpcStatus(config.artificial_validation_failure());
    }
    return absl::OkStatus();
  } catch (const Envoy::EnvoyException& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse FakeMetricsPluginConfig proto: ", e.what()));
  }
}

REGISTER_FACTORY(FakeMetricsPluginConfigFactory, MetricsPluginConfigFactory);

envoy::config::core::v3::TypedExtensionConfig MakeFakeMetricsPluginTypedExtensionConfig(
    const nighthawk::adaptive_load::FakeMetricsPluginConfig& config) {
  envoy::config::core::v3::TypedExtensionConfig outer_config;
  outer_config.set_name("nighthawk.fake_metrics_plugin");
  outer_config.mutable_typed_config()->PackFrom(config);
  return outer_config;
}

} // namespace Nighthawk
