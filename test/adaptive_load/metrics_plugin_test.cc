#include <tuple>

#include "external/envoy/source/common/config/utility.h"

#include "test/adaptive_load/utility.h"

#include "adaptive_load/metrics_plugin_impl.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

class NighthawkStatsEmulatedMetricsPluginTestFixture
    : public ::testing::TestWithParam<std::tuple<std::string, double>> {};

TEST_P(NighthawkStatsEmulatedMetricsPluginTestFixture, ComputesCorrectMetric) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"1",
          /*requests_per_second=*/1024,
          /*actual_duration_seconds=*/10,
          /*upstream_rq_total=*/2560,
          /*response_count_2xx=*/320,
          /*min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
      }));
  const std::string metric_name = std::get<0>(GetParam());
  const double value = std::get<1>(GetParam());
  EXPECT_EQ(plugin.GetMetricByName(metric_name), value);
}

INSTANTIATE_TEST_SUITE_P(
    NighthawkStatsEmulatedMetricsPluginValuesTests, NighthawkStatsEmulatedMetricsPluginTestFixture,
    ::testing::Values(std::make_tuple<std::string, double>("attempted-rps", 1024),
                      std::make_tuple<std::string, double>("achieved-rps", 256),
                      std::make_tuple<std::string, double>("send-rate", 0.25),
                      std::make_tuple<std::string, double>("success-rate", 0.125),
                      std::make_tuple<std::string, double>("latency-ns-min", 400.0),
                      std::make_tuple<std::string, double>("latency-ns-mean", 500.0),
                      std::make_tuple<std::string, double>("latency-ns-max", 600.0),
                      std::make_tuple<std::string, double>("latency-ns-mean-plus-1stdev", 511.0),
                      std::make_tuple<std::string, double>("latency-ns-mean-plus-2stdev", 522.0),
                      std::make_tuple<std::string, double>("latency-ns-mean-plus-3stdev", 533.0),
                      std::make_tuple<std::string, double>("latency-ns-pstdev", 11.0),
                      std::make_tuple<std::string, double>("nonexistent-metric-name", 0.0)));

TEST(NighthawkStatsEmulatedMetricsPluginTest, ReturnsZeroAttemptedRpsForZeroActualDuration) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"1",
          /*requests_per_second=*/1024,
          /*actual_duration_seconds=*/0,
          /*upstream_rq_total=*/2560,
          /*response_count_2xx=*/320,
          /*min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
      }));
  EXPECT_EQ(plugin.GetMetricByName("attempted-rps"), 0.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ReturnsZeroAchievedRpsForZeroActualDuration) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"1",
          /*requests_per_second=*/1024,
          /*actual_duration_seconds=*/0,
          /*upstream_rq_total=*/256,
          /*response_count_2xx=*/32,
          /*min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
      }));
  EXPECT_EQ(plugin.GetMetricByName("achieved-rps"), 0.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ReturnsZeroSendRateForZeroTotalSpecified) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"1",
          /*requests_per_second=*/0,
          /*actual_duration_seconds=*/10,
          /*upstream_rq_total=*/256,
          /*response_count_2xx=*/32,
          /*min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
      }));
  EXPECT_EQ(plugin.GetMetricByName("send-rate"), 0.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ReturnsZeroSuccessRateForZeroRequestsSent) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"1",
          /*requests_per_second=*/0,
          /*actual_duration_seconds=*/10,
          /*upstream_rq_total=*/0,
          /*response_count_2xx=*/50,
          /*min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
      }));
  EXPECT_EQ(plugin.GetMetricByName("success-rate"), 0.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, DeterminesConcurrencyWithSingleWorker) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeSimpleNighthawkOutput({
          /*concurrency=*/"auto",
          /*requests_per_second=*/123,
          /*actual_duration_seconds=*/10,
          /*upstream_rq_total=*/60,
          /*response_count_2xx=*/50,
          /*.min_ns=*/400,
          /*mean_ns=*/500,
          /*max_ns=*/600,
          /*pstdev_ns=*/11,
                }));
  // |results| contains only "global" when there was 1 worker.
  EXPECT_EQ(plugin.GetMetricByName("attempted-rps"), 123.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, DeterminesConcurrencyWithMultipleWorkers) {
  nighthawk::client::Output nighthawk_output = MakeSimpleNighthawkOutput({
      /*concurrency=*/"auto",
      /*requests_per_second=*/123,
      /*actual_duration_seconds=*/10,
            /*upstream_rq_total=*/60,
          /*response_count_2xx=*/50,
          /*.min_ns=*/400,
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
  EXPECT_EQ(plugin.GetMetricByName("attempted-rps"), 246.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ReturnsCorrectSupportedMetricNames) {
  NighthawkStatsEmulatedMetricsPlugin plugin = NighthawkStatsEmulatedMetricsPlugin({});
  std::vector<std::string> supported_metrics = plugin.GetAllSupportedMetricNames();
  EXPECT_THAT(supported_metrics,
              ::testing::ElementsAre("attempted-rps", "achieved-rps", "send-rate", "success-rate",
                                     "latency-ns-min", "latency-ns-mean", "latency-ns-max",
                                     "latency-ns-mean-plus-1stdev", "latency-ns-mean-plus-2stdev",
                                     "latency-ns-mean-plus-3stdev", "latency-ns-pstdev"));
}

} // namespace

} // namespace Nighthawk
