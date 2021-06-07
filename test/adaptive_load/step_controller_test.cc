#include "envoy/registry/registry.h"

#include "nighthawk/adaptive_load/input_variable_setter.h"

#include "external/envoy/source/common/config/utility.h"

#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/input_variable_setter_impl.pb.h"
#include "api/adaptive_load/step_controller_impl.pb.h"
#include "api/client/options.pb.h"

#include "source/adaptive_load/plugin_loader.h"
#include "source/adaptive_load/step_controller_impl.h"

#include "fake_plugins/fake_input_variable_setter/fake_input_variable_setter.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

using ::testing::HasSubstr;

nighthawk::adaptive_load::BenchmarkResult MakeBenchmarkResultWithScore(double score) {
  nighthawk::adaptive_load::BenchmarkResult result;
  nighthawk::adaptive_load::MetricEvaluation* evaluation = result.add_metric_evaluations();
  evaluation->set_threshold_score(score);
  evaluation->set_weight(10.0);
  return result;
}

TEST(ExponentialSearchStepControllerConfigFactory, GeneratesEmptyConfigProto) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.exponential_search");
  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig expected_config;
  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(ExponentialSearchStepControllerConfigFactory, CreatesCorrectFactoryName) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.exponential_search");
  EXPECT_EQ(config_factory.name(), "nighthawk.exponential_search");
}

TEST(ExponentialSearchStepControllerConfigFactory, CreatesCorrectPluginType) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  nighthawk::client::CommandLineOptions options;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.exponential_search");
  StepControllerPtr plugin = config_factory.createStepController(config_any, options);
  EXPECT_NE(dynamic_cast<ExponentialSearchStepController*>(plugin.get()), nullptr);
}

TEST(ExponentialSearchStepControllerConfigFactory,
     ValidateConfigWithoutInputVariableSetterReturnsOk) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.exponential_search");
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  absl::Status status = config_factory.ValidateConfig(config_any);
  EXPECT_TRUE(status.ok());
}

TEST(ExponentialSearchStepControllerConfigFactory,
     ValidateConfigWithValidInputVariableSetterReturnsOk) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  *config.mutable_input_variable_setter() =
      MakeFakeInputVariableSetterConfig(/*adjustment_factor=*/0);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.exponential_search");
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  absl::Status status = config_factory.ValidateConfig(config_any);
  EXPECT_TRUE(status.ok());
}

TEST(ExponentialSearchStepControllerConfigFactory,
     ValidateConfigWithInvalidInputVariableSetterReturnsError) {
  const std::string kExpectedStatusMessage = "artificial validation failure";
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  *config.mutable_input_variable_setter() = MakeFakeInputVariableSetterConfigWithValidationError(
      absl::DataLossError(kExpectedStatusMessage));
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.exponential_search");
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  absl::Status status = config_factory.ValidateConfig(config_any);
  EXPECT_EQ(status.code(), absl::StatusCode::kDataLoss);
  EXPECT_EQ(status.message(), kExpectedStatusMessage);
}

TEST(ExponentialSearchStepController, UsesInitialRps) {
  const double kInitialInput = 100.0;
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(kInitialInput);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  absl::StatusOr<nighthawk::client::CommandLineOptions> returned_options_or =
      step_controller.GetCurrentCommandLineOptions();
  ASSERT_TRUE(returned_options_or.ok());
  EXPECT_EQ(returned_options_or.value().requests_per_second().value(), kInitialInput);
}

TEST(ExponentialSearchStepController, ActivatesCustomInputVariableSetter) {
  const double kInitialInput = 100.0;
  const int kAdjustmentFactor = 123;
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig step_controller_config;
  // Sets the |connections| field in the Nighthawk input:
  *step_controller_config.mutable_input_variable_setter() =
      MakeFakeInputVariableSetterConfig(kAdjustmentFactor);
  step_controller_config.set_initial_value(kInitialInput);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(step_controller_config, options_template);
  absl::StatusOr<nighthawk::client::CommandLineOptions> returned_options_or =
      step_controller.GetCurrentCommandLineOptions();
  ASSERT_TRUE(returned_options_or.ok());
  EXPECT_EQ(returned_options_or.value().connections().value(), kInitialInput * kAdjustmentFactor);
}

TEST(ExponentialSearchStepController, PropagatesInputVariableSetterError) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig step_controller_config;
  *step_controller_config.mutable_input_variable_setter() = MakeFakeInputVariableSetterConfig(0);
  // Attempting to apply a negative value triggers an error from FakeInputVariableSetter.
  step_controller_config.set_initial_value(-1.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(step_controller_config, options_template);
  absl::StatusOr<nighthawk::client::CommandLineOptions> returned_options_or =
      step_controller.GetCurrentCommandLineOptions();
  ASSERT_FALSE(returned_options_or.ok());
  EXPECT_EQ(returned_options_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(returned_options_or.status().message(),
            "Artificial SetInputVariable failure triggered by negative value.");
}

TEST(ExponentialSearchStepController, InitiallyReportsNotConverged) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  EXPECT_FALSE(step_controller.IsConverged());
}

TEST(ExponentialSearchStepController, InitiallyReportsNotDoomed) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  std::string doom_reason = "untouched";
  EXPECT_FALSE(step_controller.IsDoomed(doom_reason));
  EXPECT_EQ(doom_reason, "untouched");
}

TEST(ExponentialSearchStepController, ReportsDoomIfOutsideThresholdsOnInitialValue) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  // Initial RPS already puts us outside metric thresholds:
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(-1.0));
  std::string doom_reason;
  EXPECT_TRUE(step_controller.IsDoomed(doom_reason));
  EXPECT_THAT(doom_reason, HasSubstr("already exceed metric thresholds with the initial load"));
}

TEST(ExponentialSearchStepController,
     IncreasesRpsExponentiallyIfWithinThresholdUsingDefaultExponent) {
  const double kInitialInput = 100.0;
  const double kDefaultExponentialFactor = 2.0;
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(kInitialInput);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(1.0));
  absl::StatusOr<nighthawk::client::CommandLineOptions> returned_options_or =
      step_controller.GetCurrentCommandLineOptions();
  ASSERT_TRUE(returned_options_or.ok());
  EXPECT_EQ(returned_options_or.value().requests_per_second().value(),
            kInitialInput * kDefaultExponentialFactor);
}

TEST(ExponentialSearchStepController,
     IncreasesRpsExponentiallyIfWithinThresholdUsingCustomExponent) {
  const double kInitialInput = 100.0;
  const double kExponentialFactor = 1.5;
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(kInitialInput);
  config.set_exponential_factor(kExponentialFactor);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(1.0));
  absl::StatusOr<nighthawk::client::CommandLineOptions> returned_options_or =
      step_controller.GetCurrentCommandLineOptions();
  ASSERT_TRUE(returned_options_or.ok());
  EXPECT_EQ(returned_options_or.value().requests_per_second().value(),
            kInitialInput * kExponentialFactor);
}

TEST(ExponentialSearchStepController, PerformsBinarySearchAfterExceedingThreshold) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  const double kInitialInput = 100.0;
  const double kDefaultExponentialFactor = 2.0;
  const double kOvershootInput = kInitialInput * kDefaultExponentialFactor;
  config.set_initial_value(kInitialInput);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(1.0));
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(-1.0));
  absl::StatusOr<nighthawk::client::CommandLineOptions> returned_options_or =
      step_controller.GetCurrentCommandLineOptions();
  ASSERT_TRUE(returned_options_or.ok());
  EXPECT_EQ(returned_options_or.value().requests_per_second().value(),
            (kInitialInput + kOvershootInput) / 2);
}

TEST(ExponentialSearchStepController, BinarySearchConvergesAfterManySteps) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(1.0));
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(-1.0));
  for (int i = 0; i < 100; ++i) {
    step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(-1.0));
  }
  EXPECT_TRUE(step_controller.IsConverged());
}

TEST(ExponentialSearchStepController, BinarySearchFindsBottomOfRange) {
  const double kInitialInput = 100.0;
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(kInitialInput);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(1.0));
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(-1.0));
  for (int i = 0; i < 100; ++i) {
    step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(-1.0));
  }
  absl::StatusOr<nighthawk::client::CommandLineOptions> returned_options_or =
      step_controller.GetCurrentCommandLineOptions();
  ASSERT_TRUE(returned_options_or.ok());
  EXPECT_EQ(returned_options_or.value().requests_per_second().value(), kInitialInput);
}

TEST(ExponentialSearchStepController, BinarySearchFindsMidpointOfRange) {
  const double kInitialInput = 100.0;
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(kInitialInput);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(1.0));
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(-1.0));
  // During binary search, succeed once to send it up to the midpoint:
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(1.0));
  // Fail every subsequent test, so it converges back down to the midpoint:
  for (int i = 0; i < 100; ++i) {
    step_controller.UpdateAndRecompute(MakeBenchmarkResultWithScore(-1.0));
  }
  absl::StatusOr<nighthawk::client::CommandLineOptions> returned_options_or =
      step_controller.GetCurrentCommandLineOptions();
  ASSERT_TRUE(returned_options_or.ok());
  EXPECT_EQ(returned_options_or.value().requests_per_second().value(), kInitialInput * 1.5);
}

} // namespace
} // namespace Nighthawk
