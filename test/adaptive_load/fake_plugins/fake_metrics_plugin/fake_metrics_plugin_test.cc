#include "adaptive_load/plugin_loader.h"
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/client/options.pb.h"
#include "envoy/registry/registry.h"
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
using ::testing::HasSubstr;
using ::Envoy::Protobuf::util::MessageDifferencer;

FakeMetricsPluginConfig
MakeFakeMetricsPluginConfigWithValidationFailureValue(absl::Status validation_error) {
  FakeMetricsPluginConfig config;
  config.mutable_artificial_validation_failure()->set_code(static_cast<int>(validation_error.code()));
  config.mutable_artificial_validation_failure()->set_message(std::string(validation_error.message()));
  return config;
}

FakeMetricsPluginConfig MakeFakeMetricsPluginConfigWithValue(absl::string_view name, double value) {
  FakeMetricsPluginConfig config;
  FakeMetricsPluginConfig::FakeMetric* fake_metric = config.mutable_fake_metrics()->Add();
  fake_metric->set_name(std::string(name));
  fake_metric->set_value(value);
  return config;
}

FakeMetricsPluginConfig MakeFakeMetricsPluginConfigWithError(absl::string_view name,
                                                             absl::Status error) {
  FakeMetricsPluginConfig config;
  FakeMetricsPluginConfig::FakeMetric* fake_metric = config.mutable_fake_metrics()->Add();
  fake_metric->set_name(std::string(name));
  fake_metric->mutable_error_status()->set_code(static_cast<int>(error.code()));
  fake_metric->mutable_error_status()->set_message(std::string(error.message()));
  return config;
}

TEST(FakeMetricsPluginConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake-metrics-plugin");
  Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  FakeMetricsPluginConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(MessageDifferencer::Equivalent(*empty_config, expected_config));
}

TEST(FakeMetricsPluginConfigFactory, FactoryRegistersUnderCorrectName) {
  FakeMetricsPluginConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake-metrics-plugin");
  EXPECT_EQ(config_factory.name(), "nighthawk.fake-metrics-plugin");
}

TEST(FakeMetricsPluginConfigFactory, CreateMetricsPluginCreatesCorrectPluginType) {
  FakeMetricsPluginConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake-metrics-plugin");
  MetricsPluginPtr plugin = config_factory.createMetricsPlugin(config_any);
  EXPECT_NE(dynamic_cast<FakeMetricsPlugin*>(plugin.get()), nullptr);
}

TEST(FakeMetricsPluginConfigFactory, ValidateConfigWithBadConfigProtoReturnsError) {
  Envoy::ProtobufWkt::Any empty_any;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake-metrics-plugin");
  absl::Status status = config_factory.ValidateConfig(empty_any);
  EXPECT_THAT(status.message(), HasSubstr("Failed to parse"));
}

TEST(FakeMetricsPluginConfigFactory, ValidateConfigWithWellFormedIllegalConfigReturnsError) {
  Envoy::ProtobufWkt::Any any;
  any.PackFrom(MakeFakeMetricsPluginConfigWithValidationFailureValue(
      absl::DeadlineExceededError("artificial validation failure")));
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake-metrics-plugin");
  absl::Status status = config_factory.ValidateConfig(any);
  EXPECT_EQ(status.code(), absl::StatusCode::kDeadlineExceeded);
  EXPECT_THAT(status.message(), "artificial validation failure");
}

TEST(FakeMetricsPlugin, GetMetricByNameReturnsValueFromConfig) {
  const double kExpectedValue = 5678.0;
  FakeMetricsPlugin metrics_plugin(
      MakeFakeMetricsPluginConfigWithValue("good-metric", kExpectedValue));
  absl::StatusOr<double> metric_value_or = metrics_plugin.GetMetricByName("good-metric");
  // ASSERT_OK(metric_value_or.status());
  ASSERT_TRUE(metric_value_or.ok());
  EXPECT_EQ(metric_value_or.value(), kExpectedValue);
}

TEST(FakeMetricsPlugin, GetMetricByNameReturnsErrorStatusFromConfig) {
  FakeMetricsPlugin metrics_plugin(
      MakeFakeMetricsPluginConfigWithError("bad-metric", absl::DataLossError("metric error")));
  absl::StatusOr<double> metric_value_or = metrics_plugin.GetMetricByName("bad-metric");
  // EXPECT_FALSE(metric_value_or.ok());
  // ASSERT_NOT_OK(metric_value_or.status());
  EXPECT_EQ(metric_value_or.status().code(), absl::StatusCode::kDataLoss);
  EXPECT_EQ(metric_value_or.status().message(), "metric error");
}

TEST(FakeMetricsPlugin, GetAllSupportedMetricNamesReturnsCorrectValues) {
  FakeMetricsPlugin metrics_plugin(MakeFakeMetricsPluginConfigWithValue("metric1", 0.0));
  EXPECT_THAT(metrics_plugin.GetAllSupportedMetricNames(), ::testing::ElementsAre("metric1"));
}

} // namespace
} // namespace Nighthawk
