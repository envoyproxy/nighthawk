#include <grpc++/grpc++.h>

#include <chrono>
#include <iostream>

#include "api/client/service.pb.h"
#include "envoy/registry/registry.h"

#include "nighthawk/adaptive_load/adaptive_load_controller.h"
#include "nighthawk/adaptive_load/input_variable_setter.h"
#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "nighthawk/adaptive_load/scoring_function.h"
#include "nighthawk/adaptive_load/step_controller.h"

#include "external/envoy/source/common/config/utility.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/input_variable_setter_impl.pb.h"
#include "api/adaptive_load/metric_spec.pb.h"
#include "api/adaptive_load/metrics_plugin_impl.pb.h"
#include "api/adaptive_load/scoring_function_impl.pb.h"
#include "api/adaptive_load/step_controller_impl.pb.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"
#include "api/client/service.grpc.pb.h"
#include "api/client/service_mock.grpc.pb.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "adaptive_load/metrics_plugin_impl.h"
#include "adaptive_load/plugin_util.h"
#include "adaptive_load/step_controller_impl.h"
#include "grpcpp/test/mock_stream.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "test/adaptive_load/utility.h"

namespace Nighthawk {
namespace AdaptiveLoad {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SetArgPointee;

// Time source that ticks 1 second on every query, starting from the Unix epoch.
class FakeTimeSource : public Envoy::TimeSource {
public:
  Envoy::SystemTime systemTime() override {
    ++unix_time_;
    Envoy::SystemTime epoch;
    return epoch + std::chrono::seconds(unix_time_);
  }
  Envoy::MonotonicTime monotonicTime() override {
    ++unix_time_;
    Envoy::MonotonicTime epoch;
    return epoch + std::chrono::seconds(unix_time_);
  }

private:
  int unix_time_{0};
};

// MetricsPlugin for testing, supporting a single metric named 'metric1' with the constant
// value 5.0.
class FakeMetricsPlugin : public MetricsPlugin {
public:
  FakeMetricsPlugin() {}
  double GetMetricByName(const std::string&) override { return 5.0; }
  const std::vector<std::string> GetAllSupportedMetricNames() const override { return {"metric1"}; }
};

// A factory that creates a FakeMetricsPlugin with no config proto, registered under the name
// 'fake-metrics-plugin'.
class FakeMetricsPluginConfigFactory : public MetricsPluginConfigFactory {
public:
  std::string name() const override { return "fake-metrics-plugin"; }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<Envoy::ProtobufWkt::Any>();
  }
  MetricsPluginPtr createMetricsPlugin(const Envoy::Protobuf::Message&) override {
    return std::make_unique<FakeMetricsPlugin>();
  }
};

REGISTER_FACTORY(FakeMetricsPluginConfigFactory, MetricsPluginConfigFactory);

static int global_convergence_countdown{};
static int global_doom_countdown{};

// StepController for testing.
class FakeStepController : public StepController {
public:
  FakeStepController() {}
  // Returns false until global_convergence_countdown reaches 0. Updates global variable
  // global_convergence_countdown.
  bool IsConverged() const override { return global_convergence_countdown-- <= 0; }
  // Returns false until global_doom_countdown reaches 0. Updates global variable
  // global_doom_countdown.
  bool IsDoomed(std::string* doomed_reason) const override {
    bool doomed = global_doom_countdown-- <= 0;
    if (doomed) {
      *doomed_reason = "fake doom reason";
    }
    return doomed;
  }
  nighthawk::client::CommandLineOptions GetCurrentCommandLineOptions() const override {
    return command_line_options_;
  }
  void UpdateAndRecompute(const nighthawk::adaptive_load::BenchmarkResult&) override {}

private:
  nighthawk::client::CommandLineOptions command_line_options_{};
};

// A factory that creates a FakeStepController with no config proto.
class FakeStepControllerConfigFactory : public StepControllerConfigFactory {
public:
  std::string name() const override { return "fake-step-controller"; }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<Envoy::ProtobufWkt::Any>();
  }
  StepControllerPtr createStepController(const Envoy::Protobuf::Message&,
                                         const nighthawk::client::CommandLineOptions&) override {
    return std::make_unique<FakeStepController>();
  }
};

REGISTER_FACTORY(FakeStepControllerConfigFactory, StepControllerConfigFactory);

// Creates a valid MetricsPluginConfig proto that activates the fake MetricsPlugin defined in this
// file.
nighthawk::adaptive_load::MetricsPluginConfig MakeFakeMetricsPluginConfig() {
  nighthawk::adaptive_load::MetricsPluginConfig config;
  config.set_name("fake-metrics-plugin");
  *config.mutable_typed_config() = Envoy::ProtobufWkt::Any();
  return config;
}

// Creates a valid StepControllerConfig proto that activates the fake StepController defined in this
// file.
nighthawk::adaptive_load::StepControllerConfig MakeFakeStepControllerConfig() {
  nighthawk::adaptive_load::StepControllerConfig config;
  config.set_name("fake-step-controller");
  *config.mutable_typed_config() = Envoy::ProtobufWkt::Any();
  return config;
}

// Creates a valid ScoringFunctionConfig proto selecting the real BinaryScoringFunction plugin
// and configuring it with a threshold.
nighthawk::adaptive_load::ScoringFunctionConfig
MakeLowerThresholdBinaryScoringFunctionConfig(double upper_threshold) {
  nighthawk::adaptive_load::ScoringFunctionConfig config;
  config.set_name("binary");

  nighthawk::adaptive_load::BinaryScoringFunctionConfig inner_config;
  inner_config.mutable_lower_threshold()->set_value(upper_threshold);
  Envoy::ProtobufWkt::Any inner_config_any;
  inner_config_any.PackFrom(inner_config);
  *config.mutable_typed_config() = inner_config_any;

  return config;
}

TEST(AdaptiveLoadControllerTest, FailsWithTrafficTemplateDurationSet) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template()->mutable_duration()->set_seconds(1);

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("should not have |duration| set"));
}

TEST(AdaptiveLoadControllerTest, FailsWithOpenLoopSet) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template()->mutable_open_loop()->set_value(false);

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("should not have |open_loop| set"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentMetricsPluginName) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  nighthawk::adaptive_load::MetricsPluginConfig* metrics_plugin_config =
      spec.mutable_metrics_plugin_configs()->Add();
  metrics_plugin_config->set_name("nonexistent-plugin");
  *metrics_plugin_config->mutable_typed_config() = Envoy::ProtobufWkt::Any();

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("MetricsPlugin not found"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentStepControllerPluginName) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  nighthawk::adaptive_load::StepControllerConfig config;
  config.set_name("nonexistent-plugin");
  *config.mutable_typed_config() = Envoy::ProtobufWkt::Any();
  *spec.mutable_step_controller_config() = config;

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("StepController plugin not found"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentScoringFunctionPluginName) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  nighthawk::adaptive_load::ScoringFunctionConfig scoring_function_config;
  scoring_function_config.set_name("nonexistent-scoring-function");
  *scoring_function_config.mutable_typed_config() = Envoy::ProtobufWkt::Any();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() = scoring_function_config;

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("ScoringFunction plugin not found"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentMetricsPluginNameInMetricThresholdSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  threshold->mutable_metric_spec()->set_metric_name("x");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nonexistent-metrics-plugin");

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(AdaptiveLoadControllerTest, FailsWithUndeclaredMetricsPluginNameInMetricThresholdSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  threshold->mutable_metric_spec()->set_metric_name("x");
  // Valid plugin name, but plugin not declared in the spec.
  threshold->mutable_metric_spec()->set_metrics_plugin_name("fake-metrics-plugin");

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentMetricsPluginNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  // spec.mutable_nighthawk_traffic_template();
  // *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("x");
  metric_spec->set_metrics_plugin_name("nonexistent-metrics-plugin");

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(AdaptiveLoadControllerTest, FailsWithUndeclaredMetricsPluginNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("x");
  // Valid plugin name, but plugin not declared in the spec.
  metric_spec->set_metrics_plugin_name("fake-metrics-plugin");

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentBuiltinMetricNameInMetricThresholdSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  threshold->mutable_metric_spec()->set_metric_name("nonexistent-metric-name");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("builtin");

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("not implemented by plugin"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentCustomMetricNameInMetricThresholdSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  threshold->mutable_metric_spec()->set_metric_name("nonexistent-metric-name");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("fake-metrics-plugin");

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("not implemented by plugin"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentBuiltinMetricNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("nonexistent-metric-name");
  metric_spec->set_metrics_plugin_name("builtin");

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("not implemented by plugin"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentCustomMetricNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();

  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("nonexistent-metric-name");
  metric_spec->set_metrics_plugin_name("fake-metrics-plugin");

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("not implemented by plugin"));
}

// Sets up a minimal working mock to be returned from the mock stub. To customize a method, start
// with the result of this function and then do another EXPECT_CALL on that method which will
// overwrite the behavior configured here.
//
// Note that this returns a bare pointer that the PerformAdaptiveLoadSession implementation must
// take ownership of.
grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                      nighthawk::client::ExecutionResponse>*
MakeSimpleMockClientReaderWriter() {
  auto* mock_reader_writer =
      new grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                                nighthawk::client::ExecutionResponse>();
  EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_reader_writer, WritesDone()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_reader_writer, Read(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_reader_writer, Finish()).WillRepeatedly(Return(::grpc::Status::OK));
  return mock_reader_writer;
}

TEST(AdaptiveLoadControllerTest, TimesOutIfNeverConverged) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  spec.mutable_convergence_deadline()->set_seconds(5);
  global_convergence_countdown = 100;
  global_doom_countdown = 100;

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(), HasSubstr("Failed to converge before deadline"));
}

TEST(AdaptiveLoadControllerTest, UsesDefaultConvergenceDeadline) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  // Not setting convergence deadline, should default to 300 seconds.

  global_convergence_countdown = 1000;
  global_doom_countdown = 1000;

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });

  FakeTimeSource time_source;
  Envoy::MonotonicTime start_time = time_source.monotonicTime();
  std::ostringstream diagnostic_ostream;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(
      std::chrono::duration_cast<std::chrono::seconds>(time_source.monotonicTime() - start_time)
          .count(),
      303); // 300 ticks plus 2 monotonicTime() calls here and 1 within the controller when it
            // recorded the start time.
}

TEST(AdaptiveLoadControllerTest, UsesDefaultMeasuringPeriod) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  global_convergence_countdown = 1000;
  global_doom_countdown = 1000;

  nighthawk::client::ExecutionRequest request;

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&request](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillRepeatedly(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        return mock_reader_writer;
      });

  FakeTimeSource time_source;
  std::ostringstream diagnostic_ostream;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  EXPECT_EQ(request.start_request().options().duration().seconds(), 10);
}

TEST(AdaptiveLoadControllerTest, UsesDefaultMetricWeight) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("metric1");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("fake-metrics-plugin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);

  global_convergence_countdown = 3;
  global_doom_countdown = 1000;

  nighthawk::client::ExecutionRequest request;

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });

  FakeTimeSource time_source;
  std::ostringstream diagnostic_ostream;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 0);
  EXPECT_EQ(
      output.adjusting_stage_results()[0].metric_evaluations()[0].threshold_spec().weight().value(),
      1.0);
}

TEST(AdaptiveLoadControllerTest, ExitsWhenDoomed) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  spec.mutable_convergence_deadline()->set_seconds(5);
  global_convergence_countdown = 1000;
  global_doom_countdown = 3;

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  EXPECT_THAT(output.session_status().message(),
              HasSubstr("Step controller determined that it can never converge"));
  EXPECT_THAT(output.session_status().message(),
              HasSubstr("fake doom reason"));
}

TEST(AdaptiveLoadControllerTest, PerformsTestingStageAfterConvergence) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  spec.mutable_convergence_deadline()->set_seconds(5);
  global_convergence_countdown = 3;
  global_doom_countdown = 1000;

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  EXPECT_TRUE(output.has_testing_stage_result());
}

TEST(AdaptiveLoadControllerTest, SetsBenchmarkErrorStatusIfNighthawkServiceDoesNotSendResponse) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  spec.mutable_convergence_deadline()->set_seconds(5);
  global_convergence_countdown = 2;
  global_doom_countdown = 1000;

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        // Simulate gRPC Read() failing:
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillRepeatedly(Return(false));
        return mock_reader_writer;
      });

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].status().code(), ::grpc::UNKNOWN);
  EXPECT_EQ(output.adjusting_stage_results()[0].status().message(),
            "Nighthawk Service did not send a response.");
}

TEST(AdaptiveLoadControllerTest,
     SetsBenchmarkErrorStatusIfNighthawkServiceGrpcStreamClosesAbnormally) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  spec.mutable_convergence_deadline()->set_seconds(5);
  global_convergence_countdown = 2;
  global_doom_countdown = 1000;

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        // Simulate gRPC abnormal stream shutdown:
        EXPECT_CALL(*mock_reader_writer, Finish())
            .WillRepeatedly(Return(::grpc::Status(::grpc::UNKNOWN, "status message")));
        return mock_reader_writer;
      });

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].status().code(), ::grpc::UNKNOWN);
  EXPECT_EQ(output.adjusting_stage_results()[0].status().message(), "status message");
}

TEST(AdaptiveLoadControllerTest, EvaluatesBuiltinMetric) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("success-rate");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("builtin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.9);

  spec.mutable_convergence_deadline()->set_seconds(5);
  global_convergence_countdown = 2;
  global_doom_countdown = 1000;

  nighthawk::client::ExecutionResponse nighthawk_service_response;
  // Success rate of 0.125.
  *nighthawk_service_response.mutable_output() = MakeStandardNighthawkOutput();

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&nighthawk_service_response](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        // Simulated Nighthawk Service output:
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillRepeatedly(DoAll(SetArgPointee<0>(nighthawk_service_response), Return(true)));
        return mock_reader_writer;
      });

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].metric_value(), 0.125);
  // Requested a lower threshold of 0.9 but only achieved 0.125.
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].threshold_score(), -1.0);
}

TEST(AdaptiveLoadControllerTest, StoresInformationalBuiltinMetric) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("success-rate");
  metric_spec->set_metrics_plugin_name("builtin");

  spec.mutable_convergence_deadline()->set_seconds(5);
  global_convergence_countdown = 2;
  global_doom_countdown = 1000;

  nighthawk::client::ExecutionResponse nighthawk_service_response;
  // Success rate of 0.125.
  *nighthawk_service_response.mutable_output() = MakeStandardNighthawkOutput();

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&nighthawk_service_response](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        // Simulated Nighthawk Service output:
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillRepeatedly(DoAll(SetArgPointee<0>(nighthawk_service_response), Return(true)));
        return mock_reader_writer;
      });

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].metric_value(), 0.125);
}

TEST(AdaptiveLoadControllerTest, EvaluatesCustomMetric) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("metric1");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("fake-metrics-plugin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(6.0);

  spec.mutable_convergence_deadline()->set_seconds(5);
  global_convergence_countdown = 2;
  global_doom_countdown = 1000;

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) {
        return MakeSimpleMockClientReaderWriter();
      });

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 0);
  // Requested a lower threshold of 6.0 but only achieved 5.0.
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].threshold_score(), -1.0);
}

TEST(AdaptiveLoadControllerTest, StoresInformationalCustomMetric) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();

  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();

  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("metric1");
  metric_spec->set_metrics_plugin_name("fake-metrics-plugin");

  spec.mutable_convergence_deadline()->set_seconds(5);
  global_convergence_countdown = 2;
  global_doom_countdown = 1000;

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) {
        return MakeSimpleMockClientReaderWriter();
      });

  std::ostringstream diagnostic_ostream;
  FakeTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub, spec, diagnostic_ostream, time_source);

  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].metric_value(), 5.0);
}

} // namespace
} // namespace AdaptiveLoad
} // namespace Nighthawk