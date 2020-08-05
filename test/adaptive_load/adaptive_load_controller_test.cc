#include <chrono>

#include "envoy/config/core/v3/base.pb.h"
#include "envoy/registry/registry.h"

#include "nighthawk/adaptive_load/adaptive_load_controller.h"
#include "nighthawk/adaptive_load/input_variable_setter.h"
#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "nighthawk/adaptive_load/scoring_function.h"
#include "nighthawk/adaptive_load/step_controller.h"

#include "external/envoy/source/common/common/statusor.h"
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
#include "api/client/service.pb.h"
#include "api/client/service_mock.grpc.pb.h"

#include "test/adaptive_load/utility.h"

#include "grpcpp/test/mock_stream.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "adaptive_load/metrics_plugin_impl.h"
#include "adaptive_load/plugin_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SetArgPointee;

/**
 * Creates a minimal Nighthawk output proto for testing:
 * - 1024 RPS attempted
 * - 10 seconds actual execution time
 * - Nighthawk was able to send 25% of requests (send-rate)
 * - 12.5% of requests sent received 2xx response (success-rate)
 * - 400/500/600/11 min/mean/max/pstdev latency ns
 */
nighthawk::client::Output MakeStandardNighthawkOutput() {
  return MakeSimpleNighthawkOutput({
      /*concurrency=*/"auto",
      /*requests_per_second=*/1024,
      /*actual_duration_seconds=*/10,
      /*upstream_rq_total=*/2560,
      /*response_count_2xx=*/320,
      /*min_ns=*/400,
      /*mean_ns=*/500,
      /*max_ns=*/600,
      /*pstdev_ns=*/11,
  });
}

/**
 * Sets up a minimal working mock to be returned from the mock Nighthawk Service stub. To customize
 * a method, start with the result of this function and then do another EXPECT_CALL on that method
 * which will overwrite the behavior configured here.
 *
 * @return Bare pointer that will be automatically wrapped in a unique_ptr by the caller.
 */
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

/**
 * MetricsPlugin for testing, supporting a single metric named 'metric1' with the constant
 * value 5.0.
 */
class FakeMetricsPlugin : public MetricsPlugin {
public:
  FakeMetricsPlugin() {}
  Envoy::StatusOr<double> GetMetricByName(absl::string_view) override { return 5.0; }
  const std::vector<std::string> GetAllSupportedMetricNames() const override { return {"metric1"}; }
};

/**
 * Factory that creates a FakeMetricsPlugin with no config proto, registered under the name
 * 'nighthawk.fake-metrics-plugin'.
 */
class FakeMetricsPluginConfigFactory : public MetricsPluginConfigFactory {
public:
  std::string name() const override { return "nighthawk.fake-metrics-plugin"; }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<Envoy::ProtobufWkt::Any>();
  }
  MetricsPluginPtr createMetricsPlugin(const Envoy::Protobuf::Message&) override {
    return std::make_unique<FakeMetricsPlugin>();
  }
};

REGISTER_FACTORY(FakeMetricsPluginConfigFactory, MetricsPluginConfigFactory);

/**
 * StepController for testing. This class is instantiated by the code under test using the Envoy
 * plugin framework, and tests do not have direct access to the instance. We assume there is only
 * one instance active at a time, and tests control it through static members. Any test that
 * uses a step controller should call one of the static initialization methods.
 */
class FakeStepController : public StepController {
public:
  FakeStepController() {}
  /**
   * Sets IsConverged to return true after the given number of ticks. Guarantees that in the event
   * of a malfunction, IsDoomed will not return true until much later. Should be called at the
   * beginning of every test that uses a step controller.
   *
   * @param ticks Number of calls to IsConverged before it should start to return true.
   */
  static void InitializeToConvergeAfterTicks(int ticks) {
    convergence_countdown_ = ticks;
    doom_countdown_ = ticks + 1000;
  }
  /**
   * Sets IsDoomed to return true after the given number of ticks. Guarantees that in the event
   * of a malfunction, IsConverged will not return true until much later. Should be called at the
   * beginning of every test that uses a step controller.
   *
   * @param ticks Number of calls to IsDoomed before it should start to return true.
   */
  static void InitializeToBeDoomedAfterTicks(int ticks) {
    doom_countdown_ = ticks;
    convergence_countdown_ = ticks + 1000;
  }
  /**
   * Returns false until convergence_countdown_ reaches 0.
   */
  bool IsConverged() const override { return convergence_countdown_-- <= 0; }
  /**
   * Returns false until doom_countdown_ reaches 0.
   */
  bool IsDoomed(std::string& doomed_reason) const override {
    bool doomed = doom_countdown_-- <= 0;
    if (doomed) {
      doomed_reason = "fake doom reason";
    }
    return doomed;
  }
  /**
   * Recommends a fixed load of 678 RPS every time.
   */
  nighthawk::client::CommandLineOptions GetCurrentCommandLineOptions() const override {
    nighthawk::client::CommandLineOptions options;
    options.mutable_requests_per_second()->set_value(678);
    return options;
  }
  void UpdateAndRecompute(const nighthawk::adaptive_load::BenchmarkResult&) override {}

private:
  static int convergence_countdown_;
  static int doom_countdown_;
};

int FakeStepController::convergence_countdown_{};
int FakeStepController::doom_countdown_{};

/**
 * Factory that creates a FakeStepController with no config proto.
 */
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

/**
 * Creates a valid TypedExtensionConfig proto that activates the fake MetricsPlugin defined in this
 * file.
 */
envoy::config::core::v3::TypedExtensionConfig MakeFakeMetricsPluginConfig() {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.fake-metrics-plugin");
  *config.mutable_typed_config() = Envoy::ProtobufWkt::Any();
  return config;
}

/**
 * Creates a valid TypedExtensionConfig proto that activates the fake StepController defined in this
 * file.
 */
envoy::config::core::v3::TypedExtensionConfig MakeFakeStepControllerConfig() {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("fake-step-controller");
  *config.mutable_typed_config() = Envoy::ProtobufWkt::Any();
  return config;
}

/**
 * Creates a valid ScoringFunctionConfig proto selecting the real BinaryScoringFunction plugin
 * and configuring it with a threshold.
 */
envoy::config::core::v3::TypedExtensionConfig
MakeLowerThresholdBinaryScoringFunctionConfig(double upper_threshold) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.binary");
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
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("should not have |duration| set"));
}

TEST(AdaptiveLoadControllerTest, FailsWithOpenLoopSet) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template()->mutable_open_loop()->set_value(false);
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("should not have |open_loop| set"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentMetricsPluginName) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  envoy::config::core::v3::TypedExtensionConfig* metrics_plugin_config =
      spec.mutable_metrics_plugin_configs()->Add();
  metrics_plugin_config->set_name("nonexistent-plugin");
  *metrics_plugin_config->mutable_typed_config() = Envoy::ProtobufWkt::Any();
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("MetricsPlugin not found"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentStepControllerPluginName) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nonexistent-plugin");
  *config.mutable_typed_config() = Envoy::ProtobufWkt::Any();
  *spec.mutable_step_controller_config() = config;
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("StepController plugin not found"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentScoringFunctionPluginName) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  envoy::config::core::v3::TypedExtensionConfig scoring_function_config;
  scoring_function_config.set_name("nonexistent-scoring-function");
  *scoring_function_config.mutable_typed_config() = Envoy::ProtobufWkt::Any();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() = scoring_function_config;
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
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
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
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
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.fake-metrics-plugin");
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentMetricsPluginNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("x");
  metric_spec->set_metrics_plugin_name("nonexistent-metrics-plugin");
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(AdaptiveLoadControllerTest, FailsWithUndeclaredMetricsPluginNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("x");
  // Valid plugin name, but plugin not declared in the spec.
  metric_spec->set_metrics_plugin_name("nighthawk.fake-metrics-plugin");
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentBuiltinMetricNameInMetricThresholdSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  threshold->mutable_metric_spec()->set_metric_name("nonexistent-metric-name");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.builtin");
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
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
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.fake-metrics-plugin");
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("not implemented by plugin"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentBuiltinMetricNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("nonexistent-metric-name");
  metric_spec->set_metrics_plugin_name("nighthawk.builtin");
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("not implemented by plugin"));
}

TEST(AdaptiveLoadControllerTest, FailsWithNonexistentCustomMetricNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("nonexistent-metric-name");
  metric_spec->set_metrics_plugin_name("nighthawk.fake-metrics-plugin");
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("not implemented by plugin"));
}

TEST(AdaptiveLoadControllerTest, TimesOutIfNeverConverged) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  spec.mutable_convergence_deadline()->set_seconds(5);
  FakeStepController::InitializeToConvergeAfterTicks(100);
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  EXPECT_THAT(output.session_status().message(), HasSubstr("Failed to converge before deadline"));
}

TEST(AdaptiveLoadControllerTest, UsesDefaultConvergenceDeadline) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  // Not setting convergence deadline, should default to 300 seconds.
  // We should hit the convergence deadline long before this convergence happens. Later we assert
  // that this was the case.
  FakeStepController::InitializeToConvergeAfterTicks(1000);
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  Envoy::MonotonicTime start_time = time_source.monotonicTime();
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  // The test assumes that it set convergence far enough in the future that the session times out
  // and skips the testing stage.
  ASSERT_FALSE(output.has_testing_stage_result());
  EXPECT_THAT(
      std::chrono::duration_cast<std::chrono::seconds>(time_source.monotonicTime() - start_time)
          .count(),
      303); // 300 ticks plus 2 monotonicTime() calls here plus 1 within the controller when it
            // recorded the start time.
}

TEST(AdaptiveLoadControllerTest, UsesDefaultTestingStageDuration) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  FakeStepController::InitializeToConvergeAfterTicks(3);
  // Successively overwritten by each adjusting stage request and finally the testing stage request.
  nighthawk::client::ExecutionRequest request;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&request](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillRepeatedly(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output.has_testing_stage_result());
  EXPECT_EQ(request.start_request().options().duration().seconds(), 30);
}

TEST(AdaptiveLoadControllerTest, UsesDefaultMeasuringPeriod) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  FakeStepController::InitializeToConvergeAfterTicks(1000);
  nighthawk::client::ExecutionRequest request;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&request](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillRepeatedly(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  EXPECT_EQ(request.start_request().options().duration().seconds(), 10);
}

TEST(AdaptiveLoadControllerTest, UsesConfiguredMeasuringPeriod) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_measuring_period()->set_seconds(17);
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  FakeStepController::InitializeToConvergeAfterTicks(1000);
  nighthawk::client::ExecutionRequest request;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&request](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillRepeatedly(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  EXPECT_EQ(request.start_request().options().duration().seconds(), 17);
}

TEST(AdaptiveLoadControllerTest, UsesCommandLineOptionsFromController) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  // Always sends 678 RPS:
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  FakeStepController::InitializeToConvergeAfterTicks(10);
  nighthawk::client::ExecutionRequest request;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&request](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillRepeatedly(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  EXPECT_EQ(request.start_request().options().requests_per_second().value(), 678);
}

TEST(AdaptiveLoadControllerTest, UsesDefaultMetricWeight) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("metric1");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.fake-metrics-plugin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  FakeStepController::InitializeToConvergeAfterTicks(3);
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].weight(), 1.0);
}

TEST(AdaptiveLoadControllerTest, UsesCustomMetricWeight) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("metric1");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.fake-metrics-plugin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  threshold->mutable_threshold_spec()->mutable_weight()->set_value(45.0);
  FakeStepController::InitializeToConvergeAfterTicks(3);
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].weight(), 45.0);
}

TEST(AdaptiveLoadControllerTest, ExitsWhenDoomed) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  spec.mutable_convergence_deadline()->set_seconds(5);
  FakeStepController::InitializeToBeDoomedAfterTicks(3);
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  EXPECT_THAT(output.session_status().message(),
              HasSubstr("Step controller determined that it can never converge"));
  EXPECT_THAT(output.session_status().message(), HasSubstr("fake doom reason"));
}

TEST(AdaptiveLoadControllerTest, PerformsTestingStageAfterConvergence) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  spec.mutable_convergence_deadline()->set_seconds(5);
  FakeStepController::InitializeToConvergeAfterTicks(3);
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  EXPECT_TRUE(output.has_testing_stage_result());
}

TEST(AdaptiveLoadControllerTest, SetsBenchmarkErrorStatusIfNighthawkServiceDoesNotSendResponse) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  spec.mutable_convergence_deadline()->set_seconds(5);
  FakeStepController::InitializeToConvergeAfterTicks(2);
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        // Simulate gRPC Read() failing:
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillRepeatedly(Return(false));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
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
  FakeStepController::InitializeToConvergeAfterTicks(2);
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        // Simulate gRPC abnormal stream shutdown:
        EXPECT_CALL(*mock_reader_writer, Finish())
            .WillRepeatedly(Return(::grpc::Status(::grpc::UNKNOWN, "status message")));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].status().code(), ::grpc::UNKNOWN);
  EXPECT_EQ(output.adjusting_stage_results()[0].status().message(), "status message");
}

TEST(AdaptiveLoadControllerTest, UsesBuiltinMetricsPluginForThresholdByDefault) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("success-rate");
  // metrics_plugin_name not set, defaults to nighthawk.builtin
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.9);
  spec.mutable_convergence_deadline()->set_seconds(5);
  FakeStepController::InitializeToConvergeAfterTicks(2);
  nighthawk::client::ExecutionResponse nighthawk_service_response;
  // Success rate of 0.125.
  *nighthawk_service_response.mutable_output() = MakeStandardNighthawkOutput();
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&nighthawk_service_response](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        // Inject simulated Nighthawk Service output:
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillRepeatedly(DoAll(SetArgPointee<0>(nighthawk_service_response), Return(true)));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].metric_value(), 0.125);
}

TEST(AdaptiveLoadControllerTest, EvaluatesBuiltinMetric) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("success-rate");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.builtin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.9);
  spec.mutable_convergence_deadline()->set_seconds(5);
  FakeStepController::InitializeToConvergeAfterTicks(2);
  nighthawk::client::ExecutionResponse nighthawk_service_response;
  // Success rate of 0.125.
  *nighthawk_service_response.mutable_output() = MakeStandardNighthawkOutput();
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&nighthawk_service_response](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        // Inject simulated Nighthawk Service output:
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillRepeatedly(DoAll(SetArgPointee<0>(nighthawk_service_response), Return(true)));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].metric_value(), 0.125);
  // Requested a lower threshold of 0.9 but only achieved 0.125.
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].threshold_score(), -1.0);
}

TEST(AdaptiveLoadControllerTest, UsesBuiltinMetricsPluginForInformationalMetricSpecByDefault) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("success-rate");
  // metrics_plugin_name not set, defaults to nighthawk.builtin
  spec.mutable_convergence_deadline()->set_seconds(5);
  FakeStepController::InitializeToConvergeAfterTicks(2);
  nighthawk::client::ExecutionResponse nighthawk_service_response;
  // Success rate of 0.125.
  *nighthawk_service_response.mutable_output() = MakeStandardNighthawkOutput();
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&nighthawk_service_response](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        // Inject simulated Nighthawk Service output:
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillRepeatedly(DoAll(SetArgPointee<0>(nighthawk_service_response), Return(true)));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].metric_value(), 0.125);
}

TEST(AdaptiveLoadControllerTest, StoresInformationalBuiltinMetric) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("success-rate");
  metric_spec->set_metrics_plugin_name("nighthawk.builtin");
  spec.mutable_convergence_deadline()->set_seconds(5);
  FakeStepController::InitializeToConvergeAfterTicks(2);
  nighthawk::client::ExecutionResponse nighthawk_service_response;
  // Success rate of 0.125.
  *nighthawk_service_response.mutable_output() = MakeStandardNighthawkOutput();
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&nighthawk_service_response](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeSimpleMockClientReaderWriter();
        // Inject simulated Nighthawk Service output:
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillRepeatedly(DoAll(SetArgPointee<0>(nighthawk_service_response), Return(true)));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
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
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.fake-metrics-plugin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(6.0);
  spec.mutable_convergence_deadline()->set_seconds(5);
  FakeStepController::InitializeToConvergeAfterTicks(2);
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
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
  metric_spec->set_metrics_plugin_name("nighthawk.fake-metrics-plugin");
  spec.mutable_convergence_deadline()->set_seconds(5);
  FakeStepController::InitializeToConvergeAfterTicks(2);
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) { return MakeSimpleMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 0);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[0].metric_value(), 5.0);
}

TEST(AdaptiveLoadControllerTest, CopiesThresholdSpecToOutput) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerConfig();
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("success-rate");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.builtin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.9);
  spec.mutable_convergence_deadline()->set_seconds(5);
  FakeStepController::InitializeToConvergeAfterTicks(2);
  nighthawk::client::ExecutionResponse nighthawk_service_response;
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
  FakeIncrementingMonotonicTimeSource time_source;
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_GT(output.metric_thresholds_size(), 0);
  EXPECT_EQ(output.metric_thresholds()[0].metric_spec().metric_name(), "success-rate");
}

} // namespace

} // namespace Nighthawk