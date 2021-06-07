#include "source/adaptive_load/metrics_evaluator_impl.h"

#include <utility>

#include "api/adaptive_load/metric_spec.pb.h"

#include "source/adaptive_load/metrics_plugin_impl.h"
#include "source/adaptive_load/plugin_loader.h"

namespace Nighthawk {

namespace {

using ::nighthawk::adaptive_load::MetricSpec;
using ::nighthawk::adaptive_load::MetricSpecWithThreshold;
using ::nighthawk::adaptive_load::ThresholdSpec;

} // namespace

absl::StatusOr<nighthawk::adaptive_load::MetricEvaluation>
MetricsEvaluatorImpl::EvaluateMetric(const MetricSpec& metric_spec, MetricsPlugin& metrics_plugin,
                                     const ThresholdSpec* threshold_spec) const {
  nighthawk::adaptive_load::MetricEvaluation evaluation;
  evaluation.set_metric_id(
      absl::StrCat(metric_spec.metrics_plugin_name(), "/", metric_spec.metric_name()));
  const absl::StatusOr<double> metric_value_or =
      metrics_plugin.GetMetricByName(metric_spec.metric_name());
  if (!metric_value_or.ok()) {
    return absl::Status(static_cast<absl::StatusCode>(metric_value_or.status().code()),
                        absl::StrCat("Error calling MetricsPlugin '",
                                     metric_spec.metrics_plugin_name(), ": ",
                                     metric_value_or.status().message()));
  }
  const double metric_value = metric_value_or.value();
  evaluation.set_metric_value(metric_value);
  if (threshold_spec == nullptr) {
    // Informational metric.
    evaluation.set_weight(0.0);
  } else {
    evaluation.set_weight(threshold_spec->weight().value());
    absl::StatusOr<ScoringFunctionPtr> scoring_function_or =
        LoadScoringFunctionPlugin(threshold_spec->scoring_function());
    RELEASE_ASSERT(scoring_function_or.ok(),
                   absl::StrCat("ScoringFunction plugin loading error should have been caught "
                                "during input validation: ",
                                scoring_function_or.status().message()));
    ScoringFunctionPtr scoring_function = std::move(scoring_function_or.value());
    evaluation.set_threshold_score(scoring_function->EvaluateMetric(metric_value));
  }
  return evaluation;
}

const std::vector<std::pair<const MetricSpec*, const ThresholdSpec*>>
MetricsEvaluatorImpl::ExtractMetricSpecs(
    const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec) const {
  std::vector<std::pair<const MetricSpec*, const ThresholdSpec*>> spec_threshold_pairs;
  for (const MetricSpecWithThreshold& metric_threshold : spec.metric_thresholds()) {
    spec_threshold_pairs.emplace_back(&metric_threshold.metric_spec(),
                                      &metric_threshold.threshold_spec());
  }
  for (const MetricSpec& metric_spec : spec.informational_metric_specs()) {
    spec_threshold_pairs.emplace_back(&metric_spec, nullptr);
  }
  return spec_threshold_pairs;
}

absl::StatusOr<nighthawk::adaptive_load::BenchmarkResult>
MetricsEvaluatorImpl::AnalyzeNighthawkBenchmark(
    const nighthawk::client::ExecutionResponse& nighthawk_response,
    const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec,
    const absl::flat_hash_map<std::string, MetricsPluginPtr>& name_to_custom_metrics_plugin_map)
    const {
  if (nighthawk_response.error_detail().code() != static_cast<int>(absl::StatusCode::kOk)) {
    return absl::Status(static_cast<absl::StatusCode>(nighthawk_response.error_detail().code()),
                        nighthawk_response.error_detail().message());
  }

  nighthawk::adaptive_load::BenchmarkResult benchmark_result;
  *benchmark_result.mutable_nighthawk_service_output() = nighthawk_response.output();

  // A map containing all available MetricsPlugins: preloaded custom plugins shared across all
  // benchmarks, and a freshly instantiated builtin plugin for this benchmark only.
  absl::flat_hash_map<std::string, MetricsPlugin*> name_to_plugin_map;
  for (const auto& name_plugin_pair : name_to_custom_metrics_plugin_map) {
    name_to_plugin_map[name_plugin_pair.first] = name_plugin_pair.second.get();
  }
  auto builtin_plugin =
      std::make_unique<NighthawkStatsEmulatedMetricsPlugin>(nighthawk_response.output());
  name_to_plugin_map["nighthawk.builtin"] = builtin_plugin.get();

  const std::vector<std::pair<const MetricSpec*, const ThresholdSpec*>> spec_threshold_pairs =
      ExtractMetricSpecs(spec);

  std::vector<std::string> errors;
  for (const std::pair<const MetricSpec*, const ThresholdSpec*>& spec_threshold_pair :
       spec_threshold_pairs) {
    absl::StatusOr<nighthawk::adaptive_load::MetricEvaluation> evaluation_or =
        EvaluateMetric(*spec_threshold_pair.first,
                       *name_to_plugin_map[spec_threshold_pair.first->metrics_plugin_name()],
                       spec_threshold_pair.second);
    if (!evaluation_or.ok()) {
      errors.emplace_back(absl::StrCat("Error evaluating metric: ", evaluation_or.status().code(),
                                       ": ", evaluation_or.status().message()));
      continue;
    }
    *benchmark_result.mutable_metric_evaluations()->Add() = evaluation_or.value();
  }
  if (!errors.empty()) {
    return absl::InternalError(absl::StrJoin(errors, "\n"));
  }
  return benchmark_result;
}

} // namespace Nighthawk
