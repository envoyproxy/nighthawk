#pragma once

#include "nighthawk/adaptive_load/metrics_evaluator.h"

#include "gmock/gmock.h"

namespace Nighthawk {

class MockMetricsEvaluator : public MetricsEvaluator {
public:
  MockMetricsEvaluator();

  MOCK_CONST_METHOD3(EvaluateMetric,
                     absl::StatusOr<nighthawk::adaptive_load::MetricEvaluation>(
                         const nighthawk::adaptive_load::MetricSpec& metric_spec,
                         MetricsPlugin& metrics_plugin,
                         const nighthawk::adaptive_load::ThresholdSpec* threshold_spec));

  MOCK_CONST_METHOD1(ExtractMetricSpecs,
                     const std::vector<std::pair<const nighthawk::adaptive_load::MetricSpec*,
                                                 const nighthawk::adaptive_load::ThresholdSpec*>>(
                         const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec));

  MOCK_CONST_METHOD3(
      AnalyzeNighthawkBenchmark,
      absl::StatusOr<nighthawk::adaptive_load::BenchmarkResult>(
          const nighthawk::client::ExecutionResponse& execution_response,
          const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec,
          const absl::flat_hash_map<std::string, MetricsPluginPtr>& name_to_custom_plugin_map));
};

} // namespace Nighthawk
