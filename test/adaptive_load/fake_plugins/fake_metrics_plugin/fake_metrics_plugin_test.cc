#include "envoy/registry/registry.h"

#include "external/envoy/source/common/config/utility.h"

#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/client/options.pb.h"

#include "test/adaptive_load/fake_plugins/fake_metrics_plugin/fake_metrics_plugin.h"
#include "test/adaptive_load/fake_plugins/fake_metrics_plugin/fake_metrics_plugin.pb.h"

#include "adaptive_load/plugin_loader.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::Envoy::Protobuf::util::MessageDifferencer;
using ::nighthawk::adaptive_load::BenchmarkResult;
using ::nighthawk::adaptive_load::FakeMetricsPluginConfig;
using ::nighthawk::adaptive_load::MetricEvaluation;
using ::nighthawk::client::CommandLineOptions;
using ::testing::HasSubstr;

TEST(FakeMetricsPluginConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake_metrics_plugin");
  Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  FakeMetricsPluginConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(MessageDifferencer::Equivalent(*empty_config, expected_config));
}

TEST(FakeMetricsPluginConfigFactory, FactoryRegistersUnderCorrectName) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake_metrics_plugin");
  EXPECT_EQ(config_factory.name(), "nighthawk.fake_metrics_plugin");
}

TEST(FakeMetricsPluginConfigFactory, CreateMetricsPluginCreatesCorrectPluginType) {
  FakeMetricsPluginConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake_metrics_plugin");
  MetricsPluginPtr plugin = config_factory.createMetricsPlugin(config_any);
  EXPECT_NE(dynamic_cast<FakeMetricsPlugin*>(plugin.get()), nullptr);
}

TEST(FakeMetricsPluginConfigFactory, ValidateConfigWithBadConfigProtoReturnsError) {
  Envoy::ProtobufWkt::Any empty_any;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake_metrics_plugin");
  absl::Status status = config_factory.ValidateConfig(empty_any);
  EXPECT_THAT(status.message(), HasSubstr("Failed to parse"));
}

TEST(FakeMetricsPluginConfigFactory, ValidateConfigWithWellFormedIllegalConfigReturnsError) {
  const int kExpectedStatusCode = static_cast<int>(absl::StatusCode::kDataLoss);
  const std::string kExpectedStatusMessage = "artificial validation failure";
  FakeMetricsPluginConfig config;
  config.mutable_artificial_validation_failure()->set_code(kExpectedStatusCode);
  config.mutable_artificial_validation_failure()->set_message(kExpectedStatusMessage);
  Envoy::ProtobufWkt::Any any;
  any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(
          "nighthawk.fake_metrics_plugin");
  absl::Status status = config_factory.ValidateConfig(any);
  EXPECT_EQ(static_cast<int>(status.code()), kExpectedStatusCode);
  EXPECT_EQ(status.message(), kExpectedStatusMessage);
}

TEST(FakeMetricsPlugin, GetMetricByNameReturnsValueFromConfig) {
  const double kExpectedValue = 5678.0;
  const std::string kMetricName = "good-metric";
  FakeMetricsPluginConfig config;
  FakeMetricsPluginConfig::FakeMetric* fake_metric = config.mutable_fake_metrics()->Add();
  fake_metric->set_name(kMetricName);
  fake_metric->set_value(kExpectedValue);
  FakeMetricsPlugin metrics_plugin(config);
  absl::StatusOr<double> metric_value_or = metrics_plugin.GetMetricByName(kMetricName);
  ASSERT_TRUE(metric_value_or.ok());
  EXPECT_EQ(metric_value_or.value(), kExpectedValue);
}

TEST(FakeMetricsPlugin, GetMetricByNameReturnsErrorStatusFromConfig) {
  const int kExpectedStatusCode = static_cast<int>(absl::StatusCode::kFailedPrecondition);
  const std::string kMetricName = "bad-metric";
  const std::string kExpectedStatusMessage = "artificial metric error";
  FakeMetricsPluginConfig config;
  FakeMetricsPluginConfig::FakeMetric* fake_metric = config.mutable_fake_metrics()->Add();
  fake_metric->set_name(kMetricName);
  fake_metric->mutable_error_status()->set_code(kExpectedStatusCode);
  fake_metric->mutable_error_status()->set_message(kExpectedStatusMessage);
  FakeMetricsPlugin metrics_plugin(config);
  absl::StatusOr<double> metric_value_or = metrics_plugin.GetMetricByName(kMetricName);
  EXPECT_EQ(static_cast<int>(metric_value_or.status().code()), kExpectedStatusCode);
  EXPECT_EQ(metric_value_or.status().message(), kExpectedStatusMessage);
}

TEST(FakeMetricsPlugin, GetAllSupportedMetricNamesReturnsCorrectValues) {
  FakeMetricsPluginConfig config;
  FakeMetricsPluginConfig::FakeMetric* fake_metric1 = config.mutable_fake_metrics()->Add();
  fake_metric1->set_name("metric1");
  FakeMetricsPluginConfig::FakeMetric* fake_metric2 = config.mutable_fake_metrics()->Add();
  fake_metric2->set_name("metric2");

  FakeMetricsPlugin metrics_plugin(config);
  EXPECT_THAT(metrics_plugin.GetAllSupportedMetricNames(),
              testing::UnorderedElementsAre("metric1", "metric2"));
}

TEST(MakeFakeMetricsPluginTypedExtensionConfig, SetsCorrectPluginName) {
  envoy::config::core::v3::TypedExtensionConfig activator =
      MakeFakeMetricsPluginTypedExtensionConfig(
          nighthawk::adaptive_load::FakeMetricsPluginConfig());
  EXPECT_EQ(activator.name(), "nighthawk.fake_metrics_plugin");
}

TEST(MakeFakeMetricsPluginTypedExtensionConfig, PacksGivenConfigProto) {
  nighthawk::adaptive_load::FakeMetricsPluginConfig expected_config;
  expected_config.mutable_fake_metrics()->Add()->set_name("a");
  envoy::config::core::v3::TypedExtensionConfig activator =
      MakeFakeMetricsPluginTypedExtensionConfig(expected_config);
  nighthawk::adaptive_load::FakeMetricsPluginConfig actual_config;
  Envoy::MessageUtil::unpackTo(activator.typed_config(), actual_config);
  EXPECT_EQ(expected_config.DebugString(), actual_config.DebugString());
  EXPECT_TRUE(MessageDifferencer::Equivalent(expected_config, actual_config));
}

} // namespace
} // namespace Nighthawk
