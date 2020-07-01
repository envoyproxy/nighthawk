#include "adaptive_rps/metrics_plugin_impl.h"

namespace Nighthawk {
namespace AdaptiveRps {

InternalMetricsPlugin::InternalMetricsPlugin(const nighthawk::client::Output& nighthawk_output) {
  for (const nighthawk::client::Result& result : nighthawk_output.results()) {
    if (result.name() != "global") {
      continue;
    }
    const int64 total_specified = nighthawk_output.options().requests_per_second().value() *
                                  nighthawk_output.options().duration().seconds();
    int64 total_sent = 0;
    int64 total_2xx = 0;
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

double InternalMetricsPlugin::GetMetricByName(const std::string& metric_name) {
  return metric_from_name_[metric_name];
}

// Note: Don't use REGISTER_FACTORY for InternalMetricsPlugin.

} // namespace AdaptiveRps
} // namespace Nighthawk
