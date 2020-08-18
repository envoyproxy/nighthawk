#include "envoy/registry/registry.h"

#include "fake_plugins/fake_input_variable_setter/fake_input_variable_setter.h"
#include "nighthawk/adaptive_load/input_variable_setter.h"

#include "external/envoy/source/common/config/utility.h"

#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/input_variable_setter_impl.pb.h"
#include "api/adaptive_load/step_controller_impl.pb.h"
#include "api/client/options.pb.h"

#include "adaptive_load/plugin_loader.h"
#include "adaptive_load/config_validator_impl.h"
#include "adaptive_load/step_controller_impl.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

nighthawk::adaptive_load::BenchmarkResult MakeBenchmarkResultOutsideThreshold() {
  nighthawk::adaptive_load::BenchmarkResult result;
  nighthawk::adaptive_load::MetricEvaluation* evaluation = result.add_metric_evaluations();
  evaluation->set_threshold_score(-1.0);
  evaluation->set_weight(10.0);
  return result;
}

nighthawk::adaptive_load::BenchmarkResult MakeBenchmarkResultWithinThreshold() {
  nighthawk::adaptive_load::BenchmarkResult result;
  nighthawk::adaptive_load::MetricEvaluation* evaluation = result.add_metric_evaluations();
  evaluation->set_threshold_score(1.0);
  evaluation->set_weight(10.0);
  return result;
}

nighthawk::adaptive_load::BenchmarkResult MakeBenchmarkResultWithNighthawkError() {
  nighthawk::adaptive_load::BenchmarkResult result;
  result.mutable_status()->set_code(1);
  return result;
}

TEST(ExponentialSearchStepControllerConfigFactory, GeneratesEmptyConfigProto) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.exponential-search");
  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig expected_config;
  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(ExponentialSearchStepControllerConfigFactory, CreatesCorrectFactoryName) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  nighthawk::client::CommandLineOptions options;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.exponential-search");
  EXPECT_EQ(config_factory.name(), "nighthawk.exponential-search");
}

TEST(ExponentialSearchStepControllerConfigFactory, CreatesCorrectPluginType) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  nighthawk::client::CommandLineOptions options;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.exponential-search");
  StepControllerPtr plugin = config_factory.createStepController(config_any, options);
  EXPECT_NE(dynamic_cast<ExponentialSearchStepController*>(plugin.get()), nullptr);
}

TEST(ExponentialSearchStepController, UsesInitialRps) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().value().requests_per_second().value(), 100);
}

TEST(ExponentialSearchStepController, ActivatesCustomInputVariableSetter) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig step_controller_config;
  // Sets the |connections| field in the Nighthawk input:
  *step_controller_config.mutable_input_variable_setter() = MakeFakeInputVariableSetterConfig(123);
  step_controller_config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(step_controller_config, options_template);
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().value().connections().value(), 12300);
}

TEST(ExponentialSearchStepController, InitiallyReportsNotConverged) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  EXPECT_FALSE(step_controller.IsConverged());
}

TEST(ExponentialSearchStepController, InitiallyReportsNotDoomed) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  std::string doom_reason = "untouched";
  EXPECT_FALSE(step_controller.IsDoomed(doom_reason));
  EXPECT_EQ(doom_reason, "untouched");
}

TEST(ExponentialSearchStepController, ReportsDoomIfOutsideThresholdsOnInitialValue) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  // Initial RPS already put us outside metric thresholds.
  step_controller.UpdateAndRecompute(MakeBenchmarkResultOutsideThreshold());
  std::string doom_reason;
  EXPECT_TRUE(step_controller.IsDoomed(doom_reason));
  EXPECT_EQ(doom_reason, "Outside threshold on initial input.");
}

TEST(ExponentialSearchStepController, ReportsDoomAfterNighthawkServiceError) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithNighthawkError());
  std::string doom_reason;
  EXPECT_TRUE(step_controller.IsDoomed(doom_reason));
  EXPECT_EQ(doom_reason, "Nighthawk Service returned an error.");
}

TEST(ExponentialSearchStepController,
     IncreasesRpsExponentiallyIfWithinThresholdUsingDefaultExponent) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithinThreshold());
  // Default exponent is 2.0.
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().value().requests_per_second().value(), 200);
}

TEST(ExponentialSearchStepController,
     IncreasesRpsExponentiallyIfWithinThresholdUsingCustomExponent) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  config.set_exponential_factor(1.5);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithinThreshold());
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().value().requests_per_second().value(), 150);
}

TEST(ExponentialSearchStepController, PerformsBinarySearchAfterExceedingThreshold) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithinThreshold());
  step_controller.UpdateAndRecompute(MakeBenchmarkResultOutsideThreshold());
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().value().requests_per_second().value(), 150);
}

TEST(ExponentialSearchStepController, BinarySearchConvergesAfterManySteps) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithinThreshold());
  step_controller.UpdateAndRecompute(MakeBenchmarkResultOutsideThreshold());
  for (int i = 0; i < 100; ++i) {
    step_controller.UpdateAndRecompute(MakeBenchmarkResultOutsideThreshold());
  }
  EXPECT_TRUE(step_controller.IsConverged());
}

TEST(ExponentialSearchStepController, BinarySearchFindsBottomOfRange) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);
  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithinThreshold());
  step_controller.UpdateAndRecompute(MakeBenchmarkResultOutsideThreshold());
  for (int i = 0; i < 100; ++i) {
    step_controller.UpdateAndRecompute(MakeBenchmarkResultOutsideThreshold());
  }
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().value().requests_per_second().value(), 100);
}

} // namespace

} // namespace Nighthawk
