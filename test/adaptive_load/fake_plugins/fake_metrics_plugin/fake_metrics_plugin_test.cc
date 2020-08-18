#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/client/options.pb.h"
#include "envoy/registry/registry.h"
#include "adaptive_load/plugin_loader.h"
#include "test/adaptive_load/fake_plugins/fake_metrics_plugin/fake_metrics_plugin.h"

#include "external/envoy/source/common/config/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::nighthawk::adaptive_load::BenchmarkResult;
using ::nighthawk::adaptive_load::FakeMetricsPluginConfig;
using ::nighthawk::adaptive_load::MetricEvaluation;
using ::nighthawk::client::CommandLineOptions;

TEST(FakeMetricsPluginConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  MetricsPluginConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake-metrics-plugin");
  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();
  FakeMetricsPluginConfig expected_config;
  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(FakeMetricsPluginConfigFactory, FactoryRegistersUnderCorrectName) {
  FakeMetricsPluginConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  CommandLineOptions options;
  MetricsPluginConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake-metrics-plugin");
  EXPECT_EQ(config_factory.name(), "nighthawk.fake-metrics-plugin");
}

TEST(FakeMetricsPluginConfigFactory, FactoryCreatesCorrectPluginType) {
  FakeMetricsPluginConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  CommandLineOptions options;
  MetricsPluginConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake-metrics-plugin");
  MetricsPluginPtr plugin = config_factory.createMetricsPlugin(config_any);
  EXPECT_NE(dynamic_cast<FakeMetricsPlugin*>(plugin.get()), nullptr);
}

TEST(FakeMetricsPlugin, GetMetricByNameReturnsDefaultValueForGoodMetricName) {
  FakeMetricsPlugin metrics_plugin(FakeMetricsPluginConfig{});
  absl::StatusOr<double> metric_value_or = metrics_plugin.GetMetricByName("good-metric");
  ASSERT_TRUE(metric_value_or.ok());
  EXPECT_EQ(metric_value_or.value(), 0.0);
}

TEST(FakeMetricsPlugin, GetMetricByNameReturnsValueFromConfigForGoodMetricName) {
  FakeMetricsPluginConfig config;
  config.set_fixed_metric_value(5678.0);
  FakeMetricsPlugin metrics_plugin(config);
  absl::StatusOr<double> metric_value_or = metrics_plugin.GetMetricByName("good-metric");
  ASSERT_TRUE(metric_value_or.ok());
  EXPECT_EQ(metric_value_or.value(), 5678.0);
}

TEST(FakeMetricsPlugin, GetMetricByNameReturnsErrorStatusForBadName) {
  FakeMetricsPluginConfig config;
  config.set_fixed_metric_value(5678.0);
  FakeMetricsPlugin metrics_plugin(config);
  absl::StatusOr<double> metric_value_or = metrics_plugin.GetMetricByName("bad-metric");
  EXPECT_FALSE(metric_value_or.ok());
  EXPECT_EQ(metric_value_or.status().message(), "bad-metric requested");
}

TEST(FakeMetricsPlugin, GetAllSupportedMetricNamesReturnsCorrectValues) {
  FakeMetricsPlugin metrics_plugin(FakeMetricsPluginConfig{});
  EXPECT_THAT(metrics_plugin.GetAllSupportedMetricNames(),
              ::testing::ElementsAre("good-metric", "bad-metric"));
}

TEST(MakeFakeMetricsPluginConfig, ActivatesFakeMetricsPlugin) {
  absl::StatusOr<MetricsPluginPtr> plugin_or = LoadMetricsPlugin(MakeFakeMetricsPluginConfig(5.0));
  ASSERT_TRUE(plugin_or.ok());
  EXPECT_NE(dynamic_cast<FakeMetricsPlugin*>(plugin_or.value().get()), nullptr);
}

TEST(MakeFakeMetricsPluginConfig, ProducesFakeMetricsPluginWithConfiguredValue) {
  absl::StatusOr<MetricsPluginPtr> plugin_or = LoadMetricsPlugin(MakeFakeMetricsPluginConfig(5.0));
  ASSERT_TRUE(plugin_or.ok());
  FakeMetricsPlugin* plugin = dynamic_cast<FakeMetricsPlugin*>(plugin_or.value().get());
  ASSERT_NE(plugin, nullptr);
  absl::StatusOr<double> value_or = plugin->GetMetricByName("good-metric");
  ASSERT_TRUE(value_or.ok());
  EXPECT_EQ(value_or.value(), 5.0);
}

} // namespace
} // namespace Nighthawk
