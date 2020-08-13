#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/client/options.pb.h"
#include "envoy/registry/registry.h"
#include "test/adaptive_load/fake_plugins/fake_step_controller/fake_step_controller_impl.h"

#include "external/envoy/source/common/config/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::nighthawk::adaptive_load::BenchmarkResult;
using ::nighthawk::adaptive_load::FakeStepControllerConfig;
using ::nighthawk::adaptive_load::MetricEvaluation;
using ::nighthawk::client::CommandLineOptions;

TEST(FakeStepControllerConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  StepControllerConfigFactory& config_factory =
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
  StepControllerConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake-step-controller");
  EXPECT_EQ(config_factory.name(), "nighthawk.fake-step-controller");
}

TEST(FakeStepControllerConfigFactory, FactoryCreatesCorrectPluginType) {
  FakeStepControllerConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  CommandLineOptions options;
  StepControllerConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
          "nighthawk.fake-step-controller");
  StepControllerPtr plugin = config_factory.createStepController(config_any, options);
  EXPECT_NE(dynamic_cast<FakeStepController*>(plugin.get()), nullptr);
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

} // namespace
} // namespace Nighthawk