#include "envoy/registry/registry.h"

#include "external/envoy/source/common/config/utility.h"

#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/client/options.pb.h"

#include "test/adaptive_load/fake_plugins/fake_step_controller/fake_step_controller.h"

#include "adaptive_load/plugin_loader.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::Envoy::Protobuf::util::MessageDifferencer;
using ::nighthawk::adaptive_load::BenchmarkResult;
using ::nighthawk::adaptive_load::FakeStepControllerConfig;
using ::nighthawk::adaptive_load::MetricEvaluation;
using ::nighthawk::client::CommandLineOptions;
using ::testing::HasSubstr;

TEST(FakeStepControllerConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake_step_controller");
  Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  FakeStepControllerConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(MessageDifferencer::Equivalent(*empty_config, expected_config));
}

TEST(FakeStepControllerConfigFactory, FactoryRegistersUnderCorrectName) {
  FakeStepControllerConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  CommandLineOptions options;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake_step_controller");
  EXPECT_EQ(config_factory.name(), "nighthawk.fake_step_controller");
}

TEST(FakeStepControllerConfigFactory, CreateStepControllerCreatesCorrectPluginType) {
  FakeStepControllerConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  CommandLineOptions options;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake_step_controller");
  StepControllerPtr plugin = config_factory.createStepController(config_any, options);
  EXPECT_NE(dynamic_cast<FakeStepController*>(plugin.get()), nullptr);
}

TEST(FakeStepControllerConfigFactory, ValidateConfigWithBadConfigProtoReturnsError) {
  Envoy::ProtobufWkt::Any empty_any;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake_step_controller");
  absl::Status status = config_factory.ValidateConfig(empty_any);
  EXPECT_THAT(status.message(), HasSubstr("Failed to parse"));
}

TEST(FakeStepControllerConfigFactory, ValidateConfigWithAritificialValidationErrorReturnsError) {
  const int kExpectedStatusCode = grpc::DATA_LOSS;
  const std::string kExpectedStatusMessage = "artificial validation error";
  FakeStepControllerConfig config;
  config.mutable_artificial_validation_failure()->set_code(kExpectedStatusCode);
  config.mutable_artificial_validation_failure()->set_message(kExpectedStatusMessage);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake_step_controller");
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
          "nighthawk.fake_step_controller");
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
          "nighthawk.fake_step_controller");
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

TEST(FakeStepController, GetCurrentCommandLineOptionsReturnsArtificialErrorImmediately) {
  FakeStepControllerConfig config;
  const int kExpectedCode = grpc::DEADLINE_EXCEEDED;
  const std::string kExpectedMessage = "artificial input setting error";
  config.mutable_artificial_input_setting_failure()->set_code(kExpectedCode);
  config.mutable_artificial_input_setting_failure()->set_message(kExpectedMessage);
  // Not setting countdown.

  FakeStepController step_controller(config, CommandLineOptions());
  absl::StatusOr<nighthawk::client::CommandLineOptions> command_line_options_or =
      step_controller.GetCurrentCommandLineOptions();
  ASSERT_FALSE(command_line_options_or.ok());
  EXPECT_EQ(static_cast<int>(command_line_options_or.status().code()), kExpectedCode);
  EXPECT_EQ(command_line_options_or.status().message(), kExpectedMessage);
}

TEST(FakeStepController, GetCurrentCommandLineOptionsReturnsArtificialErrorAfterCountdown) {
  FakeStepControllerConfig config;
  const int kExpectedCode = grpc::DEADLINE_EXCEEDED;
  const std::string kExpectedMessage = "artificial input setting error";
  config.mutable_artificial_input_setting_failure()->set_code(kExpectedCode);
  config.mutable_artificial_input_setting_failure()->set_message(kExpectedMessage);
  config.set_artificial_input_setting_failure_countdown(2);

  FakeStepController step_controller(config, CommandLineOptions());
  absl::StatusOr<nighthawk::client::CommandLineOptions> command_line_options_or1 =
      step_controller.GetCurrentCommandLineOptions();
  EXPECT_TRUE(command_line_options_or1.ok());

  step_controller.UpdateAndRecompute(nighthawk::adaptive_load::BenchmarkResult());
  // Countdown should now be 1.

  absl::StatusOr<nighthawk::client::CommandLineOptions> command_line_options_or2 =
      step_controller.GetCurrentCommandLineOptions();
  EXPECT_TRUE(command_line_options_or2.ok());

  step_controller.UpdateAndRecompute(nighthawk::adaptive_load::BenchmarkResult());
  // Countdown should now have reached 0.

  // This should now return the artificial input setting failure:
  absl::StatusOr<nighthawk::client::CommandLineOptions> command_line_options_or3 =
      step_controller.GetCurrentCommandLineOptions();
  ASSERT_FALSE(command_line_options_or3.ok());
  EXPECT_EQ(static_cast<int>(command_line_options_or3.status().code()), kExpectedCode);
  EXPECT_EQ(command_line_options_or3.status().message(), kExpectedMessage);
}

TEST(FakeStepController, IsConvergedInitiallyReturnsFalse) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  EXPECT_FALSE(step_controller.IsConverged());
}

TEST(FakeStepController, IsConvergedReturnsFalseAfterNeutralBenchmarkResult) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  step_controller.UpdateAndRecompute(benchmark_result);
  EXPECT_FALSE(step_controller.IsConverged());
}

TEST(FakeStepController, IsConvergedReturnsTrueAfterPositiveBenchmarkResultScore) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  MetricEvaluation* evaluation = benchmark_result.mutable_metric_evaluations()->Add();
  evaluation->set_threshold_score(1.0);
  step_controller.UpdateAndRecompute(benchmark_result);
  EXPECT_TRUE(step_controller.IsConverged());
}

TEST(FakeStepController, IsDoomedReturnsFalseAfterNeutralBenchmarkResult) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  step_controller.UpdateAndRecompute(benchmark_result);
  std::string doomed_reason;
  EXPECT_FALSE(step_controller.IsDoomed(doomed_reason));
}

TEST(FakeStepController,
     IsDoomedReturnsFalseAndLeavesDoomedReasonUntouchedAfterNeutralBenchmarkResult) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  step_controller.UpdateAndRecompute(benchmark_result);
  std::string variable_that_should_not_be_written = "original value";
  EXPECT_FALSE(step_controller.IsDoomed(variable_that_should_not_be_written));
  EXPECT_EQ(variable_that_should_not_be_written, "original value");
}

TEST(FakeStepController, IsDoomedReturnsTrueAndSetsDoomedReasonAfterNegativeBenchmarkResultScore) {
  FakeStepController step_controller(FakeStepControllerConfig{}, CommandLineOptions{});
  BenchmarkResult benchmark_result;
  MetricEvaluation* evaluation = benchmark_result.mutable_metric_evaluations()->Add();
  evaluation->set_threshold_score(-1.0);
  step_controller.UpdateAndRecompute(benchmark_result);
  std::string doomed_reason;
  EXPECT_TRUE(step_controller.IsDoomed(doomed_reason));
  EXPECT_EQ(doomed_reason, "artificial doom triggered by negative score");
}

TEST(MakeFakeStepControllerPluginConfig, ActivatesFakeStepControllerPlugin) {
  absl::StatusOr<StepControllerPtr> plugin_or = LoadStepControllerPlugin(
      MakeFakeStepControllerPluginConfigWithRps(0), nighthawk::client::CommandLineOptions{});
  ASSERT_TRUE(plugin_or.ok());
  EXPECT_NE(dynamic_cast<FakeStepController*>(plugin_or.value().get()), nullptr);
}

TEST(MakeFakeStepControllerPluginConfig, ProducesFakeStepControllerPluginWithConfiguredValue) {
  const int kExpectedRps = 5;
  absl::StatusOr<StepControllerPtr> plugin_or =
      LoadStepControllerPlugin(MakeFakeStepControllerPluginConfigWithRps(kExpectedRps),
                               nighthawk::client::CommandLineOptions{});
  ASSERT_TRUE(plugin_or.ok());
  auto* plugin = dynamic_cast<FakeStepController*>(plugin_or.value().get());
  ASSERT_NE(plugin, nullptr);
  absl::StatusOr<nighthawk::client::CommandLineOptions> options_or =
      plugin->GetCurrentCommandLineOptions();
  ASSERT_TRUE(options_or.ok());
  EXPECT_EQ(options_or.value().requests_per_second().value(), kExpectedRps);
}

TEST(MakeFakeStepControllerPluginConfigWithValidationError,
     ProducesFakeStepControllerPluginWithConfiguredError) {
  std::string kValidationErrorMessage = "artificial validation error";
  absl::StatusOr<StepControllerPtr> plugin_or =
      LoadStepControllerPlugin(MakeFakeStepControllerPluginConfigWithValidationError(
                                   absl::DeadlineExceededError(kValidationErrorMessage)),
                               nighthawk::client::CommandLineOptions{});
  EXPECT_EQ(plugin_or.status().code(), absl::StatusCode::kDeadlineExceeded);
  EXPECT_EQ(plugin_or.status().message(), kValidationErrorMessage);
}

TEST(MakeFakeStepControllerPluginConfigWithInputSettingError,
     ProducesFakeStepControllerPluginWithConfiguredErrorAndCountdown) {
  const int kExpectedRpsValue = 123;
  const std::string kInputSettingErrorMessage = "artificial input setting error";
  absl::StatusOr<StepControllerPtr> plugin_or = LoadStepControllerPlugin(
      MakeFakeStepControllerPluginConfigWithInputSettingError(
          kExpectedRpsValue, absl::DeadlineExceededError(kInputSettingErrorMessage),
          /*countdown=*/1),
      nighthawk::client::CommandLineOptions{});
  ASSERT_TRUE(plugin_or.ok());
  auto* plugin = dynamic_cast<FakeStepController*>(plugin_or.value().get());
  ASSERT_NE(plugin, nullptr);
  absl::StatusOr<nighthawk::client::CommandLineOptions> command_line_options_or1 =
      plugin->GetCurrentCommandLineOptions();
  ASSERT_TRUE(command_line_options_or1.ok());
  EXPECT_EQ(command_line_options_or1.value().requests_per_second().value(), kExpectedRpsValue);
  plugin->UpdateAndRecompute(BenchmarkResult());
  absl::StatusOr<nighthawk::client::CommandLineOptions> command_line_options_or2 =
      plugin->GetCurrentCommandLineOptions();
  ASSERT_FALSE(command_line_options_or2.ok());
  EXPECT_EQ(command_line_options_or2.status().code(), absl::StatusCode::kDeadlineExceeded);
  EXPECT_EQ(command_line_options_or2.status().message(), kInputSettingErrorMessage);
}

} // namespace
} // namespace Nighthawk
