#include "nighthawk/adaptive_load/metrics_evaluator.h"

namespace Nighthawk {

class MetricsEvaluatorImpl : public MetricsEvaluator {
public:
  absl::StatusOr<nighthawk::adaptive_load::MetricEvaluation>
  EvaluateMetric(const nighthawk::adaptive_load::MetricSpec& metric_spec,
                 MetricsPlugin& metrics_plugin,
                 const nighthawk::adaptive_load::ThresholdSpec* threshold_spec,
                 const MeasuringPeriod& measuring_period) const override;

  const std::vector<std::pair<const nighthawk::adaptive_load::MetricSpec*,
                              const nighthawk::adaptive_load::ThresholdSpec*>>
  ExtractMetricSpecs(const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec) const override;

  absl::StatusOr<nighthawk::adaptive_load::BenchmarkResult>
  AnalyzeNighthawkBenchmark(const nighthawk::client::ExecutionResponse& nighthawk_response,
                            const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec,
                            const absl::flat_hash_map<std::string, MetricsPluginPtr>&
                                name_to_custom_metrics_plugin_map) const override;
};

} // namespace Nighthawk
