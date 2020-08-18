#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/client/options.pb.h"
#include "envoy/registry/registry.h"
#include "adaptive_load/plugin_loader.h"
#include "test/adaptive_load/fake_plugins/fake_step_controller/fake_step_controller.h"

#include "external/envoy/source/common/config/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::nighthawk::adaptive_load::BenchmarkResult;
using ::nighthawk::adaptive_load::FakeStepControllerConfig;
using ::nighthawk::adaptive_load::MetricEvaluation;
using ::nighthawk::client::CommandLineOptions;
using ::testing::HasSubstr;

TEST(FakeStepControllerConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake-step-controller");
  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();
  FakeStepControllerConfig expected_config;
  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(FakeStepControllerConfigFactory, FactoryRegistersUnderCorrectName) {
  FakeStepControllerConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  CommandLineOptions options;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake-step-controller");
  EXPECT_EQ(config_factory.name(), "nighthawk.fake-step-controller");
}

TEST(FakeStepControllerConfigFactory, CreateMetricsPluginCreatesCorrectPluginType) {
  FakeStepControllerConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  CommandLineOptions options;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake-step-controller");
  StepControllerPtr plugin = config_factory.createStepController(config_any, options);
  EXPECT_NE(dynamic_cast<FakeStepController*>(plugin.get()), nullptr);
}

TEST(FakeStepControllerConfigFactory, ValidateConfigWithBadConfigProtoReturnsError) {
  Envoy::ProtobufWkt::Any empty_any;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake-step-controller");
  absl::Status status = config_factory.ValidateConfig(empty_any);
  EXPECT_THAT(status.message(), HasSubstr("Failed to parse"));
}

TEST(FakeStepControllerConfigFactory, ValidateConfigWithWellFormedIllegalConfigReturnsError) {
  FakeStepControllerConfig config;
  // Negative value fails config validation:
  config.set_fixed_rps_value(-1);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake-step-controller");
  absl::Status status = config_factory.ValidateConfig(config_any);
  EXPECT_THAT(status.message(), HasSubstr("Negative fixed_rps_value"));
}

TEST(FakeStepControllerConfigFactory, ValidateConfigWithDefaultConfigReturnsOk) {
  FakeStepControllerConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake-step-controller");
  absl::Status status = config_factory.ValidateConfig(config_any);
  EXPECT_TRUE(status.ok());
}

TEST(FakeStepControllerConfigFactory, ValidateConfigWithValidConfigReturnsOk) {
  FakeStepControllerConfig config;
  config.set_fixed_rps_value(1);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake-step-controller");
  absl::Status status = config_factory.ValidateConfig(config_any);
  EXPECT_TRUE(status.ok());
}

TEST(FakeStepController, GetCurrentCommandLineOptionsReturnsRpsFromConfig) {
  FakeStepControllerConfig config;
  config.set_fixed_rps_value(5678);
  FakeStepController step_controller(config, CommandLineOptions());
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().value().requests_per_second().value(),
            5678);
}

TEST(FakeStepController, IsConvergedInitiallyReturnsFalse) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  EXPECT_FALSE(step_controller.IsConverged());
}

TEST(FakeStepController, IsConvergedReturnsFalseAfterBenchmarkResultWithoutPositiveScore) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  step_controller.UpdateAndRecompute(benchmark_result);
  EXPECT_FALSE(step_controller.IsConverged());
}

TEST(FakeStepController, IsConvergedReturnsTrueAfterBenchmarkResultWithPositiveScore) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  MetricEvaluation* evaluation = benchmark_result.mutable_metric_evaluations()->Add();
  evaluation->set_threshold_score(1.0);
  step_controller.UpdateAndRecompute(benchmark_result);
  EXPECT_TRUE(step_controller.IsConverged());
}

TEST(FakeStepController, IsDoomedReturnsFalseAfterSuccessfulBenchmarkResult) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  benchmark_result.mutable_status()->set_code(::grpc::OK);
  step_controller.UpdateAndRecompute(benchmark_result);
  std::string doomed_reason;
  EXPECT_FALSE(step_controller.IsDoomed(doomed_reason));
}

TEST(FakeStepController, IsDoomedDoesNotWriteDoomedReasonAfterSuccessfulBenchmarkResult) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  benchmark_result.mutable_status()->set_code(::grpc::OK);
  step_controller.UpdateAndRecompute(benchmark_result);
  std::string doomed_reason = "untouched";
  ASSERT_FALSE(step_controller.IsDoomed(doomed_reason));
  EXPECT_EQ(doomed_reason, "untouched");
}

TEST(FakeStepController, IsDoomedReturnsTrueAfterFailedBenchmarkResult) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  benchmark_result.mutable_status()->set_code(::grpc::INTERNAL);
  benchmark_result.mutable_status()->set_message("error from nighthawk");
  step_controller.UpdateAndRecompute(benchmark_result);
  std::string doomed_reason;
  EXPECT_TRUE(step_controller.IsDoomed(doomed_reason));
}

TEST(FakeStepController, IsDoomedSetsDoomedReasonToStatusMessageAfterFailedBenchmarkResult) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  benchmark_result.mutable_status()->set_code(::grpc::INTERNAL);
  benchmark_result.mutable_status()->set_message("error from nighthawk");
  step_controller.UpdateAndRecompute(benchmark_result);
  std::string doomed_reason;
  ASSERT_TRUE(step_controller.IsDoomed(doomed_reason));
  EXPECT_EQ(doomed_reason, "error from nighthawk");
}

TEST(MakeFakeStepControllerPluginConfig, ActivatesFakeStepControllerPlugin) {
  absl::StatusOr<StepControllerPtr> plugin_or =
      LoadStepControllerPlugin(MakeFakeStepControllerPluginConfig(5), nighthawk::client::CommandLineOptions{});
  ASSERT_TRUE(plugin_or.ok());
  EXPECT_NE(dynamic_cast<FakeStepController*>(plugin_or.value().get()), nullptr);
}

TEST(MakeFakeStepControllerPluginConfig, ProducesFakeStepControllerPluginWithConfiguredValue) {
  absl::StatusOr<StepControllerPtr> plugin_or =
      LoadStepControllerPlugin(MakeFakeStepControllerPluginConfig(5), nighthawk::client::CommandLineOptions{});
  ASSERT_TRUE(plugin_or.ok());
  auto* plugin = dynamic_cast<FakeStepController*>(plugin_or.value().get());
  ASSERT_NE(plugin, nullptr);
  absl::StatusOr<nighthawk::client::CommandLineOptions> options_or =
      plugin->GetCurrentCommandLineOptions();
  ASSERT_TRUE(options_or.ok());
  EXPECT_EQ(options_or.value().requests_per_second().value(), 5);
}

} // namespace
} // namespace Nighthawk
