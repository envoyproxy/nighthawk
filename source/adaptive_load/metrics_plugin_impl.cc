#include "adaptive_load/metrics_plugin_impl.h"

#include <cmath>

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"

namespace Nighthawk {

namespace {

/**
 * Finds a Result proto with the given name within a Nighthawk Output proto.
 *
 * @param nighthawk_output The Nighthawk Output proto.
 * @param result_name The name of the desired Result, e.g. "global".
 *
 * @return StatusOr<Result> The Result with the specified name if found, otherwise an error Status.
 */
absl::StatusOr<nighthawk::client::Result>
GetResult(const nighthawk::client::Output& nighthawk_output, absl::string_view result_name) {
  for (const nighthawk::client::Result& result : nighthawk_output.results()) {
    if (result.name() == result_name) {
      return result;
    }
  }
  return absl::InternalError(
      absl::StrCat("Result '", result_name, "' not found in Nighthawk output."));
}

/**
 * Returns the value of the counter with the given name within a Result proto.
 *
 * @param result A Result proto taken from a Nighthawk Output proto.
 * @param counter_name The name of the counter, e.g. "benchmark.http_2xx".
 *
 * @return StatusOr<uint32_t> The counter value if found, otherwise an error Status.
 */
absl::StatusOr<uint32_t> GetCounter(const nighthawk::client::Result& result,
                                    absl::string_view counter_name) {
  for (const nighthawk::client::Counter& counter : result.counters()) {
    if (counter.name() == counter_name) {
      return counter.value();
    }
  }
  return absl::InternalError(
      absl::StrCat("Counter '", counter_name, "' not found in Result proto."));
}

/**
 * Finds a Statistic proto with the given id within a Result proto.
 *
 * @param result A Result proto taken from a Nighthawk Output proto.
 * @param statistic_id The name of the desired Statistic, e.g.
 * "benchmark_http_client.request_to_response".
 *
 * @return StatusOr<Statistic> The Statistic with the specified id if found, otherwise an error
 * Status.
 */
absl::StatusOr<nighthawk::client::Statistic> GetStatistic(const nighthawk::client::Result& result,
                                                          absl::string_view statistic_id) {
  for (const nighthawk::client::Statistic& statistic : result.statistics()) {
    if (statistic.id() == statistic_id) {
      return statistic;
    }
  }
  return absl::InternalError(
      absl::StrCat("Statistic '", statistic_id, "' not found in Result proto."));
}

/**
 * Extracts a value from a StatusOr into a variable if OK; otherwise appends the error message to a
 * vector. static_casts the value to fit the variable. Ignores Status code values. For use in a
 * constructor that stores a vector of errors during initialization.
 *
 * @param status_or The StatusOr to extract. The value must be static_castable to the type of
 * |value|.
 * @param value The variable where the value should be extracted. Only written if the status is OK.
 * @param errors A place to accumulate error messages. If the status is not OK, its message is
 * appended here.
 *
 * @return bool True if the status was OK.
 */
template <typename T, typename U>
bool ExtractValueOrRecordError(absl::StatusOr<T> status_or, U& value,
                               std::vector<std::string>& errors) {
  if (status_or.ok()) {
    value = static_cast<U>(status_or.value());
    return true;
  } else {
    errors.emplace_back(status_or.status().message());
    return false;
  }
}

/**
 * Extracts counters from a Nighthawk Service Output proto and computes metrics from them, storing
 * the metrics in a map.
 *
 * @param nighthawk_output An Output proto returned from Nighthawk Service.
 * @param metric_from_name A map to write computed metrics under various names.
 * @param errors A place to accumulate error messages.
 */
void ExtractCounters(const nighthawk::client::Output& nighthawk_output,
                     absl::flat_hash_map<std::string, double>& metric_from_name,
                     std::vector<std::string>& errors) {
  nighthawk::client::Result global_result;
  if (!ExtractValueOrRecordError(GetResult(nighthawk_output, "global"), global_result, errors)) {
    return;
  }
  const int64_t actual_duration_seconds =
      Envoy::Protobuf::util::TimeUtil::DurationToSeconds(global_result.execution_duration());
  // 1 worker: 'global' Result only. >1 workers: Result for each worker plus a 'global' Result.
  const uint32_t number_of_workers =
      nighthawk_output.results_size() == 1 ? 1 : nighthawk_output.results_size() - 1;
  const double total_specified =
      static_cast<double>(nighthawk_output.options().requests_per_second().value() *
                          actual_duration_seconds * number_of_workers);
  // Proceed through all calculations without crashing in order to capture all errors.
  double total_sent = std::numeric_limits<double>::quiet_NaN();
  double total_2xx = std::numeric_limits<double>::quiet_NaN();
  ExtractValueOrRecordError(GetCounter(global_result, "upstream_rq_total"), total_sent, errors);
  ExtractValueOrRecordError(GetCounter(global_result, "benchmark.http_2xx"), total_2xx, errors);
  if (actual_duration_seconds > 0.0) {
    metric_from_name["attempted-rps"] = total_specified / actual_duration_seconds;
    metric_from_name["achieved-rps"] = total_sent / actual_duration_seconds;
  } else {
    errors.emplace_back("Nighthawk returned a benchmark result with zero actual duration.");
  }
  if (total_specified > 0) {
    metric_from_name["send-rate"] = total_sent / total_specified;
  } else {
    metric_from_name["send-rate"] = 0.0;
  }
  if (total_sent > 0) {
    metric_from_name["success-rate"] = total_2xx / total_sent;
  } else {
    metric_from_name["success-rate"] = 0.0;
  }
}

/**
 * Extracts a Statistic for latency from a Nighthawk Service Output proto and computes metrics from
 * Statistic values, storing the metrics in a map.
 *
 * @param nighthawk_output An Output proto returned from Nighthawk Service.
 * @param metric_from_name A map to write computed metrics under various names.
 * @param errors A place to accumulate error messages.
 */
void ExtractStatistics(const nighthawk::client::Output& nighthawk_output,
                       absl::flat_hash_map<std::string, double>& metric_from_name,
                       std::vector<std::string>& errors) {
  nighthawk::client::Result global_result;
  if (!ExtractValueOrRecordError(GetResult(nighthawk_output, "global"), global_result, errors)) {
    return;
  }
  nighthawk::client::Statistic statistic;
  if (!ExtractValueOrRecordError(
          GetStatistic(global_result, "benchmark_http_client.request_to_response"), statistic,
          errors)) {
    return;
  }
  const double min = Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(statistic.min());
  const double mean = Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(statistic.mean());
  const double max = Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(statistic.max());
  const double pstdev = Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(statistic.pstdev());
  metric_from_name["latency-ns-min"] = min;
  metric_from_name["latency-ns-mean"] = mean;
  metric_from_name["latency-ns-max"] = max;
  metric_from_name["latency-ns-mean-plus-1stdev"] = mean + pstdev;
  metric_from_name["latency-ns-mean-plus-2stdev"] = mean + 2 * pstdev;
  metric_from_name["latency-ns-mean-plus-3stdev"] = mean + 3 * pstdev;
  metric_from_name["latency-ns-pstdev"] = pstdev;
}

} // namespace

NighthawkStatsEmulatedMetricsPlugin::NighthawkStatsEmulatedMetricsPlugin(
    const nighthawk::client::Output& nighthawk_output) {
  ExtractCounters(nighthawk_output, metric_from_name_, errors_);
  ExtractStatistics(nighthawk_output, metric_from_name_, errors_);
}

absl::StatusOr<double>
NighthawkStatsEmulatedMetricsPlugin::GetMetricByName(absl::string_view metric_name) {
  if (!errors_.empty()) {
    return absl::InternalError(absl::StrJoin(errors_, "\n"));
  }
  if (metric_from_name_.find(metric_name) == metric_from_name_.end()) {
    return absl::InternalError(
        absl::StrCat("Metric '", metric_name, "' was not computed by the 'builtin' plugin."));
  }
  return metric_from_name_[metric_name];
}

const std::vector<std::string>
NighthawkStatsEmulatedMetricsPlugin::GetAllSupportedMetricNames() const {
  return {
      "achieved-rps",
      "attempted-rps",
      "latency-ns-max",
      "latency-ns-mean",
      "latency-ns-mean-plus-1stdev",
      "latency-ns-mean-plus-2stdev",
      "latency-ns-mean-plus-3stdev",
      "latency-ns-min",
      "latency-ns-pstdev",
      "send-rate",
      "success-rate",
  };
}

// Note: Don't use REGISTER_FACTORY for NighthawkStatsEmulatedMetricsPlugin. See header for details.

} // namespace Nighthawk
