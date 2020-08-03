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
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
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

TEST(NighthawkStatsEmulatedMetricsPluginTest, ReturnsCorrectSupportedMetricNames) {
  NighthawkStatsEmulatedMetricsPlugin plugin =
      NighthawkStatsEmulatedMetricsPlugin(MakeStandardNighthawkOutput());
  std::vector<std::string> supported_metrics = plugin.GetAllSupportedMetricNames();
  EXPECT_THAT(supported_metrics,
              ::testing::ElementsAre("attempted-rps", "achieved-rps", "send-rate", "success-rate",
                                     "latency-ns-min", "latency-ns-mean", "latency-ns-max",
                                     "latency-ns-mean-plus-1stdev", "latency-ns-mean-plus-2stdev",
                                     "latency-ns-mean-plus-3stdev", "latency-ns-pstdev"));
}

} // namespace

} // namespace Nighthawk
