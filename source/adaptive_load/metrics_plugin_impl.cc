#include "adaptive_load/metrics_plugin_impl.h"

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "absl/strings/numbers.h"

namespace Nighthawk {

NighthawkStatsEmulatedMetricsPlugin::NighthawkStatsEmulatedMetricsPlugin(
    const nighthawk::client::Output& nighthawk_output) {
  for (const nighthawk::client::Result& result : nighthawk_output.results()) {
    if (result.name() != "global") {
      continue;
    }
    int concurrency;
    // "auto" is not supported since we have no way to detect the number of cores Nighthawk Service
    // used; "auto" should have been caught during input validation.
    RELEASE_ASSERT(absl::SimpleAtoi(nighthawk_output.options().concurrency().value(), &concurrency),
                   "Concurrency 'auto' not supported in Adaptive Load Controller.");
    const long total_specified = nighthawk_output.options().requests_per_second().value() *
                                 nighthawk_output.options().duration().seconds() * concurrency;
    int total_sent = 0;
    int total_2xx = 0;
    for (const nighthawk::client::Counter& counter : result.counters()) {
      if (counter.name() == "benchmark.http_2xx") {
        total_2xx = counter.value();
      } else if (counter.name() == "upstream_rq_total") {
        total_sent = counter.value();
      }
    }
    metric_from_name_["attempted-rps"] =
        total_specified / nighthawk_output.options().duration().seconds();
    metric_from_name_["achieved-rps"] =
        static_cast<double>(total_sent) / nighthawk_output.options().duration().seconds();
    metric_from_name_["send-rate"] = static_cast<double>(total_sent) / total_specified;
    metric_from_name_["success-rate"] = static_cast<double>(total_2xx) / total_sent;

    for (const nighthawk::client::Statistic& statistic : result.statistics()) {
      if (statistic.id() == "benchmark_http_client.request_to_response") {
        double min = statistic.min().seconds() * 1.0e9 + statistic.min().nanos();
        double mean = statistic.mean().seconds() * 1.0e9 + statistic.mean().nanos();
        double max = statistic.max().seconds() * 1.0e9 + statistic.max().nanos();
        double stdev = statistic.pstdev().seconds() * 1.0e9 + statistic.pstdev().nanos();
        metric_from_name_["latency-ns-min"] = min;
        metric_from_name_["latency-ns-mean"] = mean;
        metric_from_name_["latency-ns-max"] = max;
        metric_from_name_["latency-ns-mean-plus-1stdev"] = mean + stdev;
        metric_from_name_["latency-ns-mean-plus-2stdev"] = mean + 2 * stdev;
        metric_from_name_["latency-ns-mean-plus-3stdev"] = mean + 3 * stdev;
        break;
      }
    }
  }
}

double NighthawkStatsEmulatedMetricsPlugin::GetMetricByName(const std::string& metric_name) {
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
  };
}

// Note: Don't use REGISTER_FACTORY for NighthawkStatsEmulatedMetricsPlugin. See header for details.

} // namespace Nighthawk
