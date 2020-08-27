#include "envoy/config/core/v3/base.pb.h"

#include "nighthawk/adaptive_load/adaptive_load_controller.h"
#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "nighthawk/adaptive_load/scoring_function.h"
#include "nighthawk/adaptive_load/step_controller.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/metric_spec.pb.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"
#include "api/client/service.grpc.pb.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "adaptive_load/metrics_plugin_impl.h"
#include "adaptive_load/plugin_loader.h"

namespace Nighthawk {

class MetricsEvaluator {
public:
  virtual ~MetricsEvaluator() = default;

  /**
   * Given a MetricSpec, obtains a single metric value from the MetricPlugin and optionally scores
   * it according to a threshold and scoring function.
   *
   * @param metric_spec The metric spec identifying the metric by name and plugin name.
   * @param metrics_plugin An already activated MetricsPlugin used by the metric_spec.
   * @param threshold_spec Proto describing the threshold and scoring function. Nullptr if the
   * metric is informational only.
   * @param errors A vector to append error messages to.
   *
   * @return MetricEvaluation A proto containing the metric value and its score if a threshold was
   * specified, or an error mesasge if the metric could not be obtained from the MetricsPlugin.
   */
  virtual absl::StatusOr<nighthawk::adaptive_load::MetricEvaluation>
  EvaluateMetric(const nighthawk::adaptive_load::MetricSpec& metric_spec,
                 MetricsPlugin& metrics_plugin,
                 const nighthawk::adaptive_load::ThresholdSpec* threshold_spec) const PURE;

  /**
   * Extracts metric descriptors and corresponding thresholds from a top-level adaptive load session
   * spec to an ordered list and a map. Allows for uniform treatment of scored and informational
   * metrics.
   *
   * @param spec The adaptive load session spec.
   * @param metric_specs A list to store extracted MetricSpecs in order of definition.
   * @param threshold_spec_from_metric_spec A map to store each MetricSpec and its threshold if it
   * had one, or nullptr if it was an informational metric.
   *
   */
  virtual void
  ExtractMetricSpecs(const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec,
                     std::vector<const nighthawk::adaptive_load::MetricSpec*>& metric_specs,
                     absl::flat_hash_map<const nighthawk::adaptive_load::MetricSpec*,
                                         const nighthawk::adaptive_load::ThresholdSpec*>&
                         threshold_spec_from_metric_spec) const PURE;

  /**
   * Analyzes a Nighthawk Service benchmark against configured MetricThresholds. Queries
   * outside MetricsPlugins if configured and/or uses "nighthawk.builtin" plugin to extract stats
   * and counters from the Nighthawk Service output. The benchmark is assumed to have finished
   * recently so values from MetricsPlugins are relevant.
   *
   * @param nighthawk_response Proto returned from Nighthawk Service describing a single benchmark
   * session.
   * @param spec Top-level proto defining the adaptive load session.
   * @param name_to_custom_metrics_plugin_map Common map from plugin names to MetricsPlugins, loaded
   * and initialized once at the beginning of the session and passed to all calls of this function.
   *
   * @return BenchmarkResult Proto containing metric scores for this Nighthawk Service benchmark
   * session, or an error propagated from the Nighthawk Service or MetricsPlugins.
   */
  virtual absl::StatusOr<nighthawk::adaptive_load::BenchmarkResult>
  AnalyzeNighthawkBenchmark(const nighthawk::client::ExecutionResponse& nighthawk_response,
                            const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec,
                            const absl::flat_hash_map<std::string, MetricsPluginPtr>&
                                name_to_custom_metrics_plugin_map) const PURE;
};

} // namespace Nighthawk
