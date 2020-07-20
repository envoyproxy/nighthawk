#include "external/envoy/source/common/config/utility.h"

#include "adaptive_load/metrics_plugin_impl.h"
#include "adaptive_load/plugin_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/adaptive_load/utility.h"

namespace Nighthawk {
namespace AdaptiveLoad {
namespace {

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectAttemptedRps) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  double value = plugin.GetMetricByName("attempted-rps");
  EXPECT_EQ(value, 1024);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectAchievedRps) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  double value = plugin.GetMetricByName("achieved-rps");
  EXPECT_EQ(value, 256);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectSendRate) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  double value = plugin.GetMetricByName("send-rate");
  EXPECT_EQ(value, 0.25);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectSuccessRate) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  double value = plugin.GetMetricByName("success-rate");
  EXPECT_EQ(value, 0.125);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMinimumLatency) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-min");
  EXPECT_EQ(value, 400.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMeanLatency) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-mean");
  EXPECT_EQ(value, 500.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMaxLatency) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-max");
  EXPECT_EQ(value, 600.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMeanLatencyPlus1Stdev) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-mean-plus-1stdev");
  EXPECT_EQ(value, 511.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMeanLatencyPlus2Stdev) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-mean-plus-2stdev");
  EXPECT_EQ(value, 522.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ComputesCorrectMeanLatencyPlus3Stdev) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  double value = plugin.GetMetricByName("latency-ns-mean-plus-3stdev");
  EXPECT_EQ(value, 533.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ReturnsZeroForNonexistentMetric) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  double value = plugin.GetMetricByName("nonexistent-metric-name");
  EXPECT_EQ(value, 0.0);
}

TEST(NighthawkStatsEmulatedMetricsPluginTest, ReturnsCorrectSupportedMetricNames) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
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