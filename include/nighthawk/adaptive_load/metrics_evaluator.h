#include "envoy/config/core/v3/base.pb.h"

#include "nighthawk/adaptive_load/metrics_plugin.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/metric_spec.pb.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"
#include "api/client/service.pb.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"

namespace Nighthawk {

/**
 * A utility for calling MetricsPlugins and scoring metrics according to ThresholdSpecs.
 *
 * AnalyzeNighthawkBenchmark() is intended to be called repeatedly from the adaptive load controller
 * main loop after each Nighthawk Service call. The controller maintains a set of shared
 * MetricsPlugins that are initialized once for the whole session. AnalyzeNighthawkBenchmark() calls
 * EvaluateMetric() and ExtractMetricSpecs() internally. The AdaptiveLoadSessionSpec is consulted
 * for MetricSpec, ThresholdSpec, and MetricsPlugin information.
 */
class MetricsEvaluator {
public:
  virtual ~MetricsEvaluator() = default;

  /**
   * Calls a MetricPlugin to obtain the metric value defined by the MetricSpec, then scores the
   * value according to a ThresholdSpec if one is present.
   *
   * @param metric_spec The MetricSpec identifying the metric by name and plugin name.
   * @param metrics_plugin A MetricsPlugin that will be queried. The plugin must correspond to the
   * plugin name in the MetricSpec, and it should support the requested metric name in the
   * MetricSpec.
   * @param threshold_spec A proto describing the threshold and scoring function. Nullptr if the
   * metric is informational only.
   *
   * @return StatusOr<MetricEvaluation> A proto containing the metric value (and its score if a
   * threshold was specified), or an error status if the metric could not be obtained from the
   * MetricsPlugin.
   */
  virtual absl::StatusOr<nighthawk::adaptive_load::MetricEvaluation>
  EvaluateMetric(const nighthawk::adaptive_load::MetricSpec& metric_spec,
                 MetricsPlugin& metrics_plugin,
                 const nighthawk::adaptive_load::ThresholdSpec* threshold_spec) const PURE;

  /**
   * Extracts pointers to metric descriptors and corresponding thresholds from a top-level adaptive
   * load session spec to an ordered list and a map. Allows for uniform treatment of scored and
   * informational metrics.
   *
   * @param spec The adaptive load session spec.
   * @return Vector of pairs of pointers to MetricSpec and ThresholdSpec within |spec|. For
   * informational metrics, the ThresholdSpec pointer is nullptr.
   */
  virtual const std::vector<std::pair<const nighthawk::adaptive_load::MetricSpec*,
                                      const nighthawk::adaptive_load::ThresholdSpec*>>
  ExtractMetricSpecs(const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec) const PURE;

  /**
   * Analyzes a Nighthawk Service benchmark against configured MetricThresholds. For each
   * MetricSpec, queries a MetricsPlugin for the current metric value. Assumes that the values from
   * MetricsPlugins correspond timewise with the Nighthawk benchmark.
   *
   * @param nighthawk_response Proto returned from Nighthawk Service describing the latest single
   * benchmark session. To be translated into scorable metrics by the "nighthawk.builtin"
   * MetricsPlugin.
   * @param spec Top-level proto defining the adaptive load session.
   * @param name_to_custom_metrics_plugin_map Map from plugin names to initialized MetricsPlugins.
   * Must include all MetricsPlugins referenced in the spec other than "nighthawk.builtin".
   *
   * @return StatusOr<BenchmarkResult> A proto containing all metric scores for this Nighthawk
   * Service benchmark session, or an error propagated from a MetricsPlugin.
   */
  virtual absl::StatusOr<nighthawk::adaptive_load::BenchmarkResult>
  AnalyzeNighthawkBenchmark(const nighthawk::client::ExecutionResponse& nighthawk_response,
                            const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec,
                            const absl::flat_hash_map<std::string, MetricsPluginPtr>&
                                name_to_custom_metrics_plugin_map) const PURE;
};

} // namespace Nighthawk
