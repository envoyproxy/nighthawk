<<<<<<< HEAD
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/client/options.pb.h"
#include "envoy/registry/registry.h"
#include "adaptive_load/plugin_loader.h"
=======
#include "adaptive_load/plugin_loader.h"
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/client/options.pb.h"
#include "envoy/registry/registry.h"
>>>>>>> adaptive-rps-fake-step-controller
#include "test/adaptive_load/fake_plugins/fake_step_controller/fake_step_controller.h"

#include "external/envoy/source/common/config/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

<<<<<<< HEAD
=======
using ::Envoy::Protobuf::util::MessageDifferencer;
>>>>>>> adaptive-rps-fake-step-controller
using ::nighthawk::adaptive_load::BenchmarkResult;
using ::nighthawk::adaptive_load::FakeStepControllerConfig;
using ::nighthawk::adaptive_load::MetricEvaluation;
using ::nighthawk::client::CommandLineOptions;
<<<<<<< HEAD
=======
using ::testing::HasSubstr;
>>>>>>> adaptive-rps-fake-step-controller

TEST(FakeStepControllerConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake-step-controller");
<<<<<<< HEAD
  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();
  FakeStepControllerConfig expected_config;
  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
=======
  Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  FakeStepControllerConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(MessageDifferencer::Equivalent(*empty_config, expected_config));
>>>>>>> adaptive-rps-fake-step-controller
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

TEST(FakeStepControllerConfigFactory, CreateStepControllerCreatesCorrectPluginType) {
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
  const int kExpectedStatusCode = ::grpc::DATA_LOSS;
  const std::string kExpectedStatusMessage = "artificial validation error";
  FakeStepControllerConfig config;
  config.mutable_artificial_validation_failure()->set_code(kExpectedStatusCode);
  config.mutable_artificial_validation_failure()->set_message(kExpectedStatusMessage);

  config.set_fixed_rps_value(-1);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake-step-controller");
  absl::Status status = config_factory.ValidateConfig(config_any);
  EXPECT_EQ(static_cast<int>(status.code()), kExpectedStatusCode);
  EXPECT_EQ(status.message(), kExpectedStatusMessage);
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
  const int kExpectedValue = 5678;
  config.set_fixed_rps_value(kExpectedValue);
  FakeStepController step_controller(config, CommandLineOptions());
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().value().requests_per_second().value(),
            kExpectedValue);
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
  const std::string kErrorMessage = "error from nighthawk";
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  benchmark_result.mutable_status()->set_code(::grpc::INTERNAL);
  benchmark_result.mutable_status()->set_message(kErrorMessage);
  step_controller.UpdateAndRecompute(benchmark_result);
  std::string doomed_reason;
  ASSERT_TRUE(step_controller.IsDoomed(doomed_reason));
  EXPECT_EQ(doomed_reason, kErrorMessage);
}

TEST(MakeFakeStepControllerPluginConfig, ActivatesFakeStepControllerPlugin) {
  absl::StatusOr<StepControllerPtr> plugin_or = LoadStepControllerPlugin(
      MakeFakeStepControllerPluginConfig(0), nighthawk::client::CommandLineOptions{});
  ASSERT_TRUE(plugin_or.ok());
  EXPECT_NE(dynamic_cast<FakeStepController*>(plugin_or.value().get()), nullptr);
}

TEST(MakeFakeStepControllerPluginConfig, ProducesFakeStepControllerPluginWithConfiguredValue) {
  const int kExpectedRps = 5;
  absl::StatusOr<StepControllerPtr> plugin_or = LoadStepControllerPlugin(
      MakeFakeStepControllerPluginConfig(kExpectedRps), nighthawk::client::CommandLineOptions{});
  ASSERT_TRUE(plugin_or.ok());
  auto* plugin = dynamic_cast<FakeStepController*>(plugin_or.value().get());
  ASSERT_NE(plugin, nullptr);
  absl::StatusOr<nighthawk::client::CommandLineOptions> options_or =
      plugin->GetCurrentCommandLineOptions();
  ASSERT_TRUE(options_or.ok());
  EXPECT_EQ(options_or.value().requests_per_second().value(), kExpectedRps);
}

} // namespace
} // namespace Nighthawk
