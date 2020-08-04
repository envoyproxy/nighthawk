#include "adaptive_load/metrics_plugin_impl.h"

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "absl/strings/numbers.h"

namespace Nighthawk {

using ::Envoy::Protobuf::util::TimeUtil;

NighthawkStatsEmulatedMetricsPlugin::NighthawkStatsEmulatedMetricsPlugin(
    const nighthawk::client::Output& nighthawk_output) {
  // Values that could not be determined for any reason are omitted from the map.
  metric_from_name_.clear();
  for (const nighthawk::client::Result& result : nighthawk_output.results()) {
    if (result.name() != "global") {
      continue;
    }

    const int64_t actual_duration_seconds =
        TimeUtil::DurationToSeconds(result.execution_duration());
    const uint32_t number_of_workers =
        nighthawk_output.results_size() == 1 ? 1 : nighthawk_output.results_size() - 1;
    const long total_specified = nighthawk_output.options().requests_per_second().value() *
                                 actual_duration_seconds * number_of_workers;
    int total_sent = 0;
    int total_2xx = 0;
    for (const nighthawk::client::Counter& counter : result.counters()) {
      if (counter.name() == "benchmark.http_2xx") {
        total_2xx = counter.value();
      } else if (counter.name() == "upstream_rq_total") {
        total_sent = counter.value();
      }
    }
    if (actual_duration_seconds > 0.0) {
      metric_from_name_["attempted-rps"] =
          static_cast<double>(total_specified) / actual_duration_seconds;
      metric_from_name_["achieved-rps"] = static_cast<double>(total_sent) / actual_duration_seconds;
    } else {
      ENVOY_LOG(warn, "Nighthawk returned a benchmark result with zero actual duration.");
    }
    if (total_specified > 0) {
      metric_from_name_["send-rate"] =
          static_cast<double>(total_sent) / static_cast<double>(total_specified);
    }
    if (total_sent > 0) {
      metric_from_name_["success-rate"] =
          static_cast<double>(total_2xx) / static_cast<double>(total_sent);
    }

    for (const nighthawk::client::Statistic& statistic : result.statistics()) {
      if (statistic.id() == "benchmark_http_client.request_to_response") {
        const double min = TimeUtil::DurationToNanoseconds(statistic.min());
        const double mean = TimeUtil::DurationToNanoseconds(statistic.mean());
        const double max = TimeUtil::DurationToNanoseconds(statistic.max());
        const double pstdev = TimeUtil::DurationToNanoseconds(statistic.pstdev());
        metric_from_name_["latency-ns-min"] = min;
        metric_from_name_["latency-ns-mean"] = mean;
        metric_from_name_["latency-ns-max"] = max;
        metric_from_name_["latency-ns-mean-plus-1stdev"] = mean + pstdev;
        metric_from_name_["latency-ns-mean-plus-2stdev"] = mean + 2 * pstdev;
        metric_from_name_["latency-ns-mean-plus-3stdev"] = mean + 3 * pstdev;
        metric_from_name_["latency-ns-pstdev"] = pstdev;
        break;
      }
    }
  }
}

double NighthawkStatsEmulatedMetricsPlugin::GetMetricByName(const std::string& metric_name) {
  // Values that could not be obtained will be missing from the map and will produce 0.0.
  return metric_from_name_[metric_name];
}

const std::vector<std::string>
NighthawkStatsEmulatedMetricsPlugin::GetAllSupportedMetricNames() const {
  return {
      "attempted-rps",
      "achieved-rps",
      "send-rate",
      "success-rate",
      "latency-ns-min",
      "latency-ns-mean",
      "latency-ns-max",
      "latency-ns-mean-plus-1stdev",
      "latency-ns-mean-plus-2stdev",
      "latency-ns-mean-plus-3stdev",
      "latency-ns-pstdev",
  };
}

// Note: Don't use REGISTER_FACTORY for NighthawkStatsEmulatedMetricsPlugin. See header for details.

} // namespace Nighthawk
