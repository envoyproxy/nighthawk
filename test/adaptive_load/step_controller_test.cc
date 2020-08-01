#include "envoy/registry/registry.h"

#include "nighthawk/adaptive_load/input_variable_setter.h"

#include "external/envoy/source/common/config/utility.h"

#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/input_variable_setter_impl.pb.h"
#include "api/adaptive_load/step_controller_impl.pb.h"
#include "api/client/options.pb.h"

#include "adaptive_load/plugin_util.h"
#include "adaptive_load/step_controller_impl.h"
#include "google/rpc/code.pb.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

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
  result.mutable_status()->set_code(google::rpc::Code::UNKNOWN);
  return result;
}

// Non-default InputVariableSetter for testing.
class ConnectionsInputVariableSetter : public InputVariableSetter {
public:
  ConnectionsInputVariableSetter() {}
  void SetInputVariable(nighthawk::client::CommandLineOptions& command_line_options,
                        double input_value) override {
    command_line_options.mutable_connections()->set_value(static_cast<unsigned int>(input_value));
  }
};

// A factory that creates a ConnectionsInputVariableSetter from a
// ConnectionsInputVariableSetterConfig proto.
class ConnectionsInputVariableSetterConfigFactory : public InputVariableSetterConfigFactory {
public:
  std::string name() const override { return "nighthawk.testing-connections"; }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<Envoy::ProtobufWkt::Any>();
  }
  InputVariableSetterPtr createInputVariableSetter(const Envoy::Protobuf::Message&) override {
    return std::make_unique<ConnectionsInputVariableSetter>();
  }
};

REGISTER_FACTORY(ConnectionsInputVariableSetterConfigFactory, InputVariableSetterConfigFactory);

TEST(ExponentialSearchStepControllerConfigFactoryTest, GeneratesEmptyConfigProto) {
  StepControllerConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.exponential-search");

  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();

  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig expected_config;

  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(ExponentialSearchStepControllerConfigFactoryTest, CreatesPlugin) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);

  nighthawk::client::CommandLineOptions options;

  StepControllerConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.exponential-search");
  StepControllerPtr plugin = config_factory.createStepController(config_any, options);

  EXPECT_NE(dynamic_cast<ExponentialSearchStepController*>(plugin.get()), nullptr);
}

TEST(ExponentialSearchStepControllerTest, UsesInitialRps) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);

  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().requests_per_second().value(), 100);
}

TEST(ExponentialSearchStepControllerTest, ActivatesCustomInputValueSetter) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig step_controller_config;
  step_controller_config.mutable_input_variable_setter()->set_name("nighthawk.testing-connections");
  // typed_config can be set to any valid Any proto, as the test-only
  // ConnectionsInputVariableSetterConfigFactory defined above ignores the config proto.
  *step_controller_config.mutable_input_variable_setter()->mutable_typed_config() =
      Envoy::ProtobufWkt::Any();

  step_controller_config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(step_controller_config, options_template);

  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().connections().value(), 100);
}

TEST(ExponentialSearchStepControllerTest, InitiallyReportsNotConverged) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);

  EXPECT_FALSE(step_controller.IsConverged());
}

TEST(ExponentialSearchStepControllerTest, InitiallyReportsNotDoomed) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);

  std::string doom_reason = "untouched";
  EXPECT_FALSE(step_controller.IsDoomed(&doom_reason));
  EXPECT_EQ(doom_reason, "untouched");
}

TEST(ExponentialSearchStepControllerTest, ReportsDoomIfOutsideThresholdsOnInitialValue) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);

  // Initial RPS already put us outside metric thresholds.
  step_controller.UpdateAndRecompute(MakeBenchmarkResultOutsideThreshold());

  std::string doom_reason;
  EXPECT_TRUE(step_controller.IsDoomed(&doom_reason));
  EXPECT_EQ(doom_reason, "Outside threshold on initial input.");
}

TEST(ExponentialSearchStepControllerTest, ReportsDoomAfterNighthawkServiceError) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);

  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithNighthawkError());

  std::string doom_reason;
  EXPECT_TRUE(step_controller.IsDoomed(&doom_reason));
  EXPECT_EQ(doom_reason, "Nighthawk Service returned an error.");
}

TEST(ExponentialSearchStepControllerTest,
     IncreasesRpsExponentiallyIfWithinThresholdUsingDefaultExponent) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);

  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithinThreshold());
  // Default exponent is 2.0.
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().requests_per_second().value(), 200);
}

TEST(ExponentialSearchStepControllerTest,
     IncreasesRpsExponentiallyIfWithinThresholdUsingCustomExponent) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  config.set_exponential_factor(1.5);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);

  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithinThreshold());
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().requests_per_second().value(), 150);
}

TEST(ExponentialSearchStepControllerTest, PerformsBinarySearchAfterExceedingThreshold) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);

  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithinThreshold());
  step_controller.UpdateAndRecompute(MakeBenchmarkResultOutsideThreshold());
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().requests_per_second().value(), 150);
}

TEST(ExponentialSearchStepControllerTest, BinarySearchConvergesAfterManySteps) {
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

TEST(ExponentialSearchStepControllerTest, BinarySearchFindsBottomOfRange) {
  nighthawk::adaptive_load::ExponentialSearchStepControllerConfig config;
  config.set_initial_value(100.0);
  nighthawk::client::CommandLineOptions options_template;
  ExponentialSearchStepController step_controller(config, options_template);

  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithinThreshold());
  step_controller.UpdateAndRecompute(MakeBenchmarkResultOutsideThreshold());
  for (int i = 0; i < 100; ++i) {
    step_controller.UpdateAndRecompute(MakeBenchmarkResultOutsideThreshold());
  }
  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().requests_per_second().value(), 100);
}

TEST(FixedSequenceStepControllerConfigFactoryTest, GeneratesEmptyConfigProto) {
  StepControllerConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fixed-sequence");

  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();

  nighthawk::adaptive_load::FixedSequenceStepControllerConfig expected_config;

  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(FixedSequenceStepControllerConfigFactoryTest, CreatesPlugin) {
  nighthawk::adaptive_load::FixedSequenceStepControllerConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);

  nighthawk::client::CommandLineOptions options;

  StepControllerConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fixed-sequence");
  StepControllerPtr plugin = config_factory.createStepController(config_any, options);

  EXPECT_NE(dynamic_cast<FixedSequenceStepController*>(plugin.get()), nullptr);
}

TEST(FixedSequenceStepControllerTest, ActivatesCustomInputValueSetter) {
  nighthawk::adaptive_load::FixedSequenceStepControllerConfig config;
  config.mutable_input_variable_setter()->set_name("nighthawk.testing-connections");
  // typed_config can be set to any valid Any proto, as the test-only
  // ConnectionsInputVariableSetterConfigFactory defined above ignores the config proto.
  *config.mutable_input_variable_setter()->mutable_typed_config() =
      Envoy::ProtobufWkt::Any();

  config.add_input_values(100.0);
  config.add_input_values(200.0);
  config.add_input_values(300.0);
  nighthawk::client::CommandLineOptions options_template;
  FixedSequenceStepController step_controller(config, options_template);

  EXPECT_EQ(step_controller.GetCurrentCommandLineOptions().connections().value(), 100);
}

TEST(FixedSequenceStepControllerTest, InitiallyReportsNotConverged) {
  nighthawk::adaptive_load::FixedSequenceStepControllerConfig config;
  config.add_input_values(100.0);
  config.add_input_values(200.0);
  config.add_input_values(300.0);
  nighthawk::client::CommandLineOptions options_template;
  FixedSequenceStepController step_controller(config, options_template);

  EXPECT_FALSE(step_controller.IsConverged());
}

TEST(FixedSequenceStepControllerTest, InitiallyReportsNotDoomed) {
  nighthawk::adaptive_load::FixedSequenceStepControllerConfig config;
  config.add_input_values(100.0);
  config.add_input_values(200.0);
  config.add_input_values(300.0);
  nighthawk::client::CommandLineOptions options_template;
  FixedSequenceStepController step_controller(config, options_template);

  std::string doom_reason = "untouched";
  EXPECT_FALSE(step_controller.IsDoomed(&doom_reason));
  EXPECT_EQ(doom_reason, "untouched");
}

TEST(FixedSequenceStepControllerTest, ReportsDoomIfNoInputValuesProvided) {
  nighthawk::adaptive_load::FixedSequenceStepControllerConfig config;
  // No input values added.
  nighthawk::client::CommandLineOptions options_template;
  FixedSequenceStepController step_controller(config, options_template);

  std::string doom_reason;
  EXPECT_TRUE(step_controller.IsDoomed(&doom_reason));
  EXPECT_EQ(doom_reason, "FixedSequenceStepController requires at least one value in input_values.");
}

TEST(FixedSequenceStepControllerTest, ReportsDoomAfterNighthawkServiceError) {
  nighthawk::adaptive_load::FixedSequenceStepControllerConfig config;
  config.add_input_values(100.0);
  config.add_input_values(200.0);
  config.add_input_values(300.0);
  nighthawk::client::CommandLineOptions options_template;
  FixedSequenceStepController step_controller(config, options_template);

  step_controller.UpdateAndRecompute(MakeBenchmarkResultWithNighthawkError());

  std::string doom_reason;
  EXPECT_TRUE(step_controller.IsDoomed(&doom_reason));
  EXPECT_EQ(doom_reason, "Nighthawk Service returned an error.");
}

TEST(FixedSequenceStepControllerTest, PerformsCorrectSequence) {
  nighthawk::adaptive_load::FixedSequenceStepControllerConfig config;
  config.add_input_values(100.0);
  config.add_input_values(200.0);
  config.add_input_values(300.0);
  nighthawk::client::CommandLineOptions options_template;
  FixedSequenceStepController step_controller(config, options_template);

  std::vector<double> results;
  for (int i = 0; i < 5; ++i) {
    results.push_back(step_controller.GetCurrentCommandLineOptions().requests_per_second().value());
    step_controller.UpdateAndRecompute(MakeBenchmarkResultWithinThreshold());
  }
  EXPECT_THAT(results, ::testing::ElementsAre(100.0, 200.0, 300.0, 300.0, 300.0));
}

TEST(FixedSequenceStepControllerTest, ReportsConvergenceAtEndOfSequence) {
  nighthawk::adaptive_load::FixedSequenceStepControllerConfig config;
  config.add_input_values(100.0);
  config.add_input_values(200.0);
  config.add_input_values(300.0);
  nighthawk::client::CommandLineOptions options_template;
  FixedSequenceStepController step_controller(config, options_template);

  std::vector<bool> results;
  for (int i = 0; i < 5; ++i) {
    results.push_back(step_controller.IsConverged());
    step_controller.UpdateAndRecompute(MakeBenchmarkResultWithinThreshold());
  }
  EXPECT_THAT(results, ::testing::ElementsAre(false, false, false, true, true));
}

} // namespace

} // namespace Nighthawk