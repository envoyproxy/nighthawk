#include "source/adaptive_load/session_spec_proto_helper_impl.h"

#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "nighthawk/adaptive_load/step_controller.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/adaptive_load.pb.validate.h"
#include "api/adaptive_load/metric_spec.pb.h"

#include "source/adaptive_load/metrics_plugin_impl.h"
#include "source/adaptive_load/plugin_loader.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace Nighthawk {

nighthawk::adaptive_load::AdaptiveLoadSessionSpec
AdaptiveLoadSessionSpecProtoHelperImpl::SetSessionSpecDefaults(
    nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec) const {
  if (!spec.nighthawk_traffic_template().has_open_loop()) {
    spec.mutable_nighthawk_traffic_template()->mutable_open_loop()->set_value(true);
  }
  if (!spec.has_measuring_period()) {
    spec.mutable_measuring_period()->set_seconds(10);
  }
  if (!spec.has_convergence_deadline()) {
    spec.mutable_convergence_deadline()->set_seconds(300);
  }
  if (!spec.has_testing_stage_duration()) {
    spec.mutable_testing_stage_duration()->set_seconds(30);
  }
  for (nighthawk::adaptive_load::MetricSpecWithThreshold& threshold :
       *spec.mutable_metric_thresholds()) {
    if (threshold.metric_spec().metrics_plugin_name().empty()) {
      threshold.mutable_metric_spec()->set_metrics_plugin_name("nighthawk.builtin");
    }
    if (!threshold.threshold_spec().has_weight()) {
      threshold.mutable_threshold_spec()->mutable_weight()->set_value(1.0);
    }
  }
  for (nighthawk::adaptive_load::MetricSpec& metric_spec :
       *spec.mutable_informational_metric_specs()) {
    if (metric_spec.metrics_plugin_name().empty()) {
      metric_spec.set_metrics_plugin_name("nighthawk.builtin");
    }
  }
  return spec;
}

absl::Status AdaptiveLoadSessionSpecProtoHelperImpl::CheckSessionSpec(
    const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec) const {
  std::vector<std::string> errors;
  if (spec.nighthawk_traffic_template().has_duration()) {
    errors.emplace_back(
        "nighthawk_traffic_template should not have |duration| set. Set |measuring_period| "
        "and |testing_stage_duration| in the AdaptiveLoadSessionSpec proto instead.");
  }

  {
    std::string validation_error;
    if (!Validate(spec, &validation_error)) {
      errors.push_back(
          absl::StrCat("the AdaptiveLoadSessionSpec doesn't validate: ", validation_error));
    }
  }

  absl::flat_hash_map<std::string, MetricsPluginPtr> plugin_from_name;
  std::vector<std::string> plugin_names = {"nighthawk.builtin"};
  plugin_from_name["nighthawk.builtin"] =
      std::make_unique<NighthawkStatsEmulatedMetricsPlugin>(nighthawk::client::Output());
  for (const envoy::config::core::v3::TypedExtensionConfig& config :
       spec.metrics_plugin_configs()) {
    plugin_names.push_back(config.name());
    absl::StatusOr<MetricsPluginPtr> metrics_plugin_or = LoadMetricsPlugin(config);
    if (!metrics_plugin_or.ok()) {
      errors.emplace_back(
          absl::StrCat("Failed to load MetricsPlugin: ", metrics_plugin_or.status().message()));
      continue;
    }
    plugin_from_name[config.name()] = std::move(metrics_plugin_or.value());
  }
  absl::StatusOr<StepControllerPtr> step_controller_or =
      LoadStepControllerPlugin(spec.step_controller_config(), spec.nighthawk_traffic_template());
  if (!step_controller_or.ok()) {
    errors.emplace_back(absl::StrCat("Failed to load StepController plugin: ",
                                     step_controller_or.status().message()));
  }
  std::vector<nighthawk::adaptive_load::MetricSpec> all_metric_specs;
  for (const nighthawk::adaptive_load::MetricSpecWithThreshold& metric_threshold :
       spec.metric_thresholds()) {
    all_metric_specs.push_back(metric_threshold.metric_spec());
    absl::StatusOr<ScoringFunctionPtr> scoring_function_or =
        LoadScoringFunctionPlugin(metric_threshold.threshold_spec().scoring_function());
    if (!scoring_function_or.ok()) {
      errors.emplace_back(absl::StrCat("Failed to load ScoringFunction plugin: ",
                                       scoring_function_or.status().message()));
    }
  }
  for (const nighthawk::adaptive_load::MetricSpec& metric_spec :
       spec.informational_metric_specs()) {
    all_metric_specs.push_back(metric_spec);
  }
  for (const nighthawk::adaptive_load::MetricSpec& metric_spec : all_metric_specs) {
    if (plugin_from_name.contains(metric_spec.metrics_plugin_name())) {
      std::vector<std::string> supported_metrics =
          plugin_from_name[metric_spec.metrics_plugin_name()]->GetAllSupportedMetricNames();
      if (std::find(supported_metrics.begin(), supported_metrics.end(),
                    metric_spec.metric_name()) == supported_metrics.end()) {
        errors.emplace_back(
            absl::StrCat("Metric named '", metric_spec.metric_name(),
                         "' not implemented by plugin '", metric_spec.metrics_plugin_name(),
                         "'. Metrics implemented: ", absl::StrJoin(supported_metrics, ", "), "."));
      }
    } else {
      errors.emplace_back(absl::StrCat(
          "MetricSpec referred to nonexistent metrics_plugin_name '",
          metric_spec.metrics_plugin_name(),
          "'. You must declare the plugin in metrics_plugin_configs or use plugin ",
          "'nighthawk.builtin'. Available plugins: ", absl::StrJoin(plugin_names, ", "), "."));
    }
  }
  if (errors.size() > 0) {
    return absl::InvalidArgumentError(absl::StrJoin(errors, "\n"));
  }
  return absl::OkStatus();
}

} // namespace Nighthawk
