#include "adaptive_load/metrics_plugin_impl.h"
#include "adaptive_load/plugin_util.h"
#include "external/envoy/source/common/config/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace AdaptiveLoad {
namespace {

void SetCounterValue(nighthawk::client::Counter* counter, const std::string& name, int value) {
  counter->set_name(name);
  counter->set_value(value);
}

void SetStatisticValues(nighthawk::client::Statistic* statistic, const std::string& id, long min_ns,
                        long mean_ns, long max_ns, long pstdev_ns) {
  const long kOneBillion = 1000 * 1000 * 1000;
  statistic->set_id(id);
  statistic->mutable_min()->set_seconds(min_ns / kOneBillion);
  statistic->mutable_min()->set_nanos(min_ns % kOneBillion);
  statistic->mutable_mean()->set_seconds(mean_ns / kOneBillion);
  statistic->mutable_mean()->set_nanos(mean_ns % kOneBillion);
  statistic->mutable_max()->set_seconds(max_ns / kOneBillion);
  statistic->mutable_max()->set_nanos(max_ns % kOneBillion);
  statistic->mutable_pstdev()->set_seconds(pstdev_ns / kOneBillion);
  statistic->mutable_pstdev()->set_nanos(pstdev_ns % kOneBillion);
}

nighthawk::client::Output MakeNighthawkOutput() {
  nighthawk::client::Output output;
  output.mutable_options()->mutable_requests_per_second()->set_value(1024);
  output.mutable_options()->mutable_duration()->set_seconds(10);
  nighthawk::client::Result* result = output.mutable_results()->Add();
  result->set_name("global");
  // 1/4 of requests were successfully sent.
  SetCounterValue(result->mutable_counters()->Add(), "upstream_rq_total", 2560);
  // 1/8 of successfully sent requests returned 2xx.
  SetCounterValue(result->mutable_counters()->Add(), "benchmark.http_2xx", 320);
  SetStatisticValues(result->mutable_statistics()->Add(),
                     "benchmark_http_client.request_to_response", /*min_ns=*/400, /*mean_ns=*/500,
                     /*max_ns=*/600, /*pstdev_ns=*/11);
  return output;
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectAttemptedRps) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  double value = plugin.GetMetricByName("attempted-rps");
  EXPECT_EQ(value, 1024);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectAchievedRps) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  double value = plugin.GetMetricByName("achieved-rps");
  EXPECT_EQ(value, 256);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectSendRate) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  double value = plugin.GetMetricByName("send-rate");
  EXPECT_EQ(value, 0.25);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectSuccessRate) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  double value = plugin.GetMetricByName("success-rate");
  EXPECT_EQ(value, 0.125);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMinimumLatency) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-min");
  EXPECT_EQ(value, 400.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMeanLatency) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-mean");
  EXPECT_EQ(value, 500.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMaxLatency) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-max");
  EXPECT_EQ(value, 600.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMeanLatencyPlus1Stdev) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-mean-plus-1stdev");
  EXPECT_EQ(value, 511.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMeanLatencyPlus2Stdev) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-mean-plus-2stdev");
  EXPECT_EQ(value, 522.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMeanLatencyPlus3Stdev) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-mean-plus-3stdev");
  EXPECT_EQ(value, 533.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ReturnsZeroForNonexistentMetric) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  double value = plugin.GetMetricByName("nonexistent-metric-name");
  EXPECT_EQ(value, 0.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ReturnsCorrectSupportedMetricNames) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeNighthawkOutput());
  std::vector<std::string> supported_metrics = plugin.GetAllSupportedMetricNames();
  EXPECT_THAT(supported_metrics,
              ::testing::ElementsAre("attempted-rps", "achieved-rps", "send-rate", "success-rate",
                                     "latency-ns-min", "latency-ns-mean", "latency-ns-max",
                                     "latency-ns-mean-plus-1stdev", "latency-ns-mean-plus-2stdev",
                                     "latency-ns-mean-plus-3stdev"));
}

} // namespace
} // namespace AdaptiveLoad
} // namespace Nighthawk