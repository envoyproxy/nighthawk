#include <tuple>

#include "external/envoy/source/common/config/utility.h"

#include "test/adaptive_load/minimal_output.h"

#include "adaptive_load/metrics_plugin_impl.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

class NighthawkStatsEmulatedMetricsPluginFixture
    : public testing::TestWithParam<std::tuple<std::string, double>> {};

TEST_P(NighthawkStatsEmulatedMetricsPluginFixture, ComputesCorrectMetric) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"auto",
          /*requests_per_second=*/1024,
          /*actual_duration_seconds=*/10,
          /*upstream_rq_total=*/2560,
          /*response_count_2xx=*/320,
          /*min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
      }));
  const std::string& metric_name = std::get<0>(GetParam());
  const double value = std::get<1>(GetParam());
  EXPECT_EQ(plugin.GetMetricByName(metric_name).value(), value);
}

INSTANTIATE_TEST_SUITE_P(
    NighthawkStatsEmulatedMetricsPluginValuesTests, NighthawkStatsEmulatedMetricsPluginFixture,
    testing::Values(std::make_tuple<std::string, double>("achieved-rps", 256),
                    std::make_tuple<std::string, double>("attempted-rps", 1024),
                    std::make_tuple<std::string, double>("latency-ns-max", 600.0),
                    std::make_tuple<std::string, double>("latency-ns-mean", 500.0),
                    std::make_tuple<std::string, double>("latency-ns-mean-plus-1stdev", 511.0),
                    std::make_tuple<std::string, double>("latency-ns-mean-plus-2stdev", 522.0),
                    std::make_tuple<std::string, double>("latency-ns-mean-plus-3stdev", 533.0),
                    std::make_tuple<std::string, double>("latency-ns-min", 400.0),
                    std::make_tuple<std::string, double>("latency-ns-pstdev", 11.0),
                    std::make_tuple<std::string, double>("send-rate", 0.25),
                    std::make_tuple<std::string, double>("success-rate", 0.125)));

TEST(NighthawkStatsEmulatedMetricsPlugin, ReturnsErrorIfGlobalResultMissing) {
  nighthawk::client::Output empty_output;
  NighthawkStatsEmulatedMetricsPlugin plugin = NighthawkStatsEmulatedMetricsPlugin(empty_output);
  EXPECT_THAT(plugin.GetMetricByName("x").status().message(),
              testing::HasSubstr("'global' not found"));
}

TEST(NighthawkStatsEmulatedMetricsPlugin, ReturnsErrorIf2xxMissing) {
  nighthawk::client::Output output = MakeSimpleNighthawkOutput({
      /*concurrency=*/"auto",
      /*requests_per_second=*/1024,
      /*actual_duration_seconds=*/10,
      /*upstream_rq_total=*/2560,
      /*response_count_2xx=*/320,
      /*min_ns=*/400,
      /*mean_ns=*/500,
      /*max_ns=*/600,
      /*pstdev_ns=*/11,
  });
  output.mutable_results(0)->clear_counters();
  NighthawkStatsEmulatedMetricsPlugin plugin = NighthawkStatsEmulatedMetricsPlugin(output);
  EXPECT_THAT(plugin.GetMetricByName("x").status().message(),
              testing::HasSubstr("'benchmark.http_2xx' not found"));
}

TEST(NighthawkStatsEmulatedMetricsPlugin, ReturnsErrorIfUpstreamRqTotalMissing) {
  nighthawk::client::Output output = MakeSimpleNighthawkOutput({
      /*concurrency=*/"auto",
      /*requests_per_second=*/1024,
      /*actual_duration_seconds=*/10,
      /*upstream_rq_total=*/2560,
      /*response_count_2xx=*/320,
      /*min_ns=*/400,
      /*mean_ns=*/500,
      /*max_ns=*/600,
      /*pstdev_ns=*/11,
  });
  output.mutable_results(0)->clear_counters();
  NighthawkStatsEmulatedMetricsPlugin plugin = NighthawkStatsEmulatedMetricsPlugin(output);
  EXPECT_THAT(plugin.GetMetricByName("x").status().message(),
              testing::HasSubstr("'upstream_rq_total' not found"));
}

TEST(NighthawkStatsEmulatedMetricsPlugin, ReturnsErrorForZeroActualDuration) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"auto",
          /*requests_per_second=*/1024,
          /*actual_duration_seconds=*/0,
          /*upstream_rq_total=*/2560,
          /*response_count_2xx=*/320,
          /*min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
      }));
  EXPECT_THAT(plugin.GetMetricByName("x").status().message(),
              testing::HasSubstr("zero actual duration"));
}

TEST(NighthawkStatsEmulatedMetricsPlugin, ReturnsErrorIfStatisticMissing) {
  nighthawk::client::Output output = MakeSimpleNighthawkOutput({
      /*concurrency=*/"auto",
      /*requests_per_second=*/1024,
      /*actual_duration_seconds=*/10,
      /*upstream_rq_total=*/2560,
      /*response_count_2xx=*/320,
      /*min_ns=*/400,
      /*mean_ns=*/500,
      /*max_ns=*/600,
      /*pstdev_ns=*/11,
  });
  output.mutable_results(0)->clear_statistics();
  NighthawkStatsEmulatedMetricsPlugin plugin = NighthawkStatsEmulatedMetricsPlugin(output);
  EXPECT_THAT(plugin.GetMetricByName("x").status().message(),
              testing::HasSubstr("'benchmark_http_client.request_to_response' not found"));
}

TEST(NighthawkStatsEmulatedMetricsPlugin, ReturnsZeroSuccessRateForZeroRequestsSent) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"auto",
          /*requests_per_second=*/1024,
          /*actual_duration_seconds=*/10,
          /*upstream_rq_total=*/0,
          /*response_count_2xx=*/320,
          /*min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
      }));
  EXPECT_EQ(plugin.GetMetricByName("success-rate").value(), 0.0);
}

TEST(NighthawkStatsEmulatedMetricsPlugin, ReturnsZeroSendRateForZeroTotalSpecified) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"auto",
          /*requests_per_second=*/0,
          /*actual_duration_seconds=*/10,
          /*upstream_rq_total=*/2560,
          /*response_count_2xx=*/320,
          /*min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
      }));
  EXPECT_EQ(plugin.GetMetricByName("send-rate").value(), 0.0);
}

TEST(NighthawkStatsEmulatedMetricsPlugin, ReturnsErrorForNonexistentMetricName) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"auto",
          /*requests_per_second=*/123,
          /*actual_duration_seconds=*/10,
          /*upstream_rq_total=*/2560,
          /*response_count_2xx=*/320,
          /*min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
      }));
  EXPECT_THAT(plugin.GetMetricByName("nonexistent-metric-name").status().message(),
              testing::HasSubstr("was not computed by the 'builtin' plugin"));
}

TEST(NighthawkStatsEmulatedMetricsPlugin, DeterminesConcurrencyWithSingleWorker) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"auto",
          /*requests_per_second=*/123,
          /*actual_duration_seconds=*/10,
          /*upstream_rq_total=*/2560,
          /*response_count_2xx=*/320,
          /*min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
      }));
  // |results| in output contains only "global" when there was 1 worker.
  EXPECT_EQ(plugin.GetMetricByName("attempted-rps").value(), 123.0);
}

TEST(NighthawkStatsEmulatedMetricsPlugin, DeterminesConcurrencyWithMultipleWorkers) {
  nighthawk::client::Output nighthawk_output = MakeSimpleNighthawkOutput({
      /*concurrency=*/"auto",
      /*requests_per_second=*/123,
      /*actual_duration_seconds=*/10,
      /*upstream_rq_total=*/2560,
      /*response_count_2xx=*/320,
      /*min_ns=*/400,
      /*mean_ns=*/500,
      /*max_ns=*/600,
      /*pstdev_ns=*/11,
  });

  // |results| when there are 2 workers contains 3 entries not necessarily in this order: "global"
  // merged results, worker 1 individual results, worker 2 individual results.
  *nighthawk_output.mutable_results()->Add() = nighthawk_output.results(0);
  *nighthawk_output.mutable_results(1)->mutable_name() = "worker1";
  *nighthawk_output.mutable_results()->Add() = nighthawk_output.results(0);
  *nighthawk_output.mutable_results(2)->mutable_name() = "worker2";
  ASSERT_EQ(nighthawk_output.results_size(), 3);

  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(nighthawk_output);
  EXPECT_EQ(plugin.GetMetricByName("attempted-rps").value(), 246.0);
}

TEST(NighthawkStatsEmulatedMetricsPlugin, ReturnsCorrectSupportedMetricNames) {
  NighthawkStatsEmulatedMetricsPlugin plugin = NighthawkStatsEmulatedMetricsPlugin({});
  std::vector<std::string> supported_metrics = plugin.GetAllSupportedMetricNames();
  EXPECT_THAT(supported_metrics,
              testing::ElementsAre("achieved-rps", "attempted-rps", "latency-ns-max",
                                   "latency-ns-mean", "latency-ns-mean-plus-1stdev",
                                   "latency-ns-mean-plus-2stdev", "latency-ns-mean-plus-3stdev",
                                   "latency-ns-min", "latency-ns-pstdev", "send-rate",
                                   "success-rate"));
}

} // namespace

} // namespace Nighthawk
