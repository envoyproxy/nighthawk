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

#include "grpcpp/test/mock_stream.h"

#include "test/adaptive_load/fake_plugins/fake_metrics_plugin/fake_metrics_plugin.h"
#include "test/adaptive_load/fake_plugins/fake_step_controller/fake_step_controller.h"
#include "test/adaptive_load/fake_time_source.h"
#include "test/adaptive_load/minimal_output.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "adaptive_load/metrics_plugin_impl.h"
#include "adaptive_load/plugin_loader.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

using ::nighthawk::adaptive_load::AdaptiveLoadSessionOutput;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SetArgPointee;

/**
 * Creates a valid TypedExtensionConfig proto selecting the real BinaryScoringFunction plugin
 * and configuring it with a threshold.
 *
 * @param lower_threshold Threshold value to set within the config proto.
 *
 * @return TypedExtensionConfig Full scoring function plugin spec that selects
 * nighthawk.binary-scoring and provides a config.
 */
envoy::config::core::v3::TypedExtensionConfig
MakeLowerThresholdBinaryScoringFunctionConfig(double lower_threshold) {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.binary_scoring");
  nighthawk::adaptive_load::BinaryScoringFunctionConfig inner_config;
  inner_config.mutable_lower_threshold()->set_value(lower_threshold);
  config.mutable_typed_config()->PackFrom(inner_config);
  return config;
}

/**
 * Creates a session spec with BuiltinMetricsPlugin configured to collect send-rate, a
 * BinaryScoringFunction with a lower threshold of 0.9, and a FakeStepController, which
 * will report convergence whenever the metric score of a mock Nighthawk Service response evaluates
 * to a positive value, or doom whenever the mock Nighthawk Service returns an error code. This
 * session spec may be amended or overwritten.
 *
 * @return AdaptiveLoadSessionSpec Adaptive load session spec proto.
 */
nighthawk::adaptive_load::AdaptiveLoadSessionSpec MakeConvergeableDoomableSessionSpec() {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template();
  *spec.mutable_step_controller_config() =
      MakeFakeStepControllerPluginConfig(/*fixed_rps_value=*/1024);
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("send-rate");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.builtin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.9);
  return spec;
}

envoy::config::core::v3::TypedExtensionConfig MakeFakeMetricsPluginConfig() {
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nighthawk.fake_metrics_plugin");
  nighthawk::adaptive_load::FakeMetricsPluginConfig inner_config;
  nighthawk::adaptive_load::FakeMetricsPluginConfig::FakeMetric* fake_metric_good =
      inner_config.mutable_fake_metrics()->Add();
  fake_metric_good->set_name("good_metric");
  fake_metric_good->set_value(5.0);
  nighthawk::adaptive_load::FakeMetricsPluginConfig::FakeMetric* fake_metric_bad =
      inner_config.mutable_fake_metrics()->Add();
  fake_metric_bad->set_name("bad_metric");
  fake_metric_bad->mutable_error_status()->set_code(::grpc::INTERNAL);
  fake_metric_bad->mutable_error_status()->set_message("bad_metric simulated error");
  config.mutable_typed_config()->PackFrom(inner_config);
  return config;
}

/**
 * Creates a simulated Nighthawk Service response that reflects the specified send rate. Combined
 * with BuiltinMetricsPlugin and BinaryScoringFunction with a lower threshold, this can be used to
 * produce a 'send-rate' metric score of 1.0 or -1.0 on demand. This in turn can be used to make
 * FakeStepController report convergence for testing purposes, by arrranging a metric score of 1.0.
 *
 * For example, use the response MakeNighthawkResponseWithSendRate(1.0) and a lower threshold of
 * 0.9 to produce the score 1.0, or the response MakeNighthawkResponseWithSendRate(0.5) with the
 * same threshold to produce the score -1.0.
 *
 * @return ExecutionResponse A simulated Nighthawk Service response with counters representing the
 * specified send rate, along with other dummy counters and stats.
 */
nighthawk::client::ExecutionResponse MakeNighthawkResponseWithSendRate(double send_rate) {
  nighthawk::client::ExecutionResponse response;
  nighthawk::client::Output output = MakeSimpleNighthawkOutput({
      /*concurrency=*/"auto",
      /*requests_per_second=*/1024,
      /*actual_duration_seconds=*/10,
      /*upstream_rq_total=*/static_cast<int>(10 * 1024 * send_rate),
      /*response_count_2xx=*/320,
      /*min_ns=*/400,
      /*mean_ns=*/500,
      /*max_ns=*/600,
      /*pstdev_ns=*/11,
  });
  *response.mutable_output() = output;
  return response;
}

/**
 * Creates a simulated Nighthawk Service response with an error code. FakeStepController becomes
 * doomed when it receives this response.
 *
 * @return ExecutionResponse A Nighthawk Service response containing an error code.
 */
nighthawk::client::ExecutionResponse MakeFailedNighthawkResponse() {
  nighthawk::client::ExecutionResponse response;
  response.mutable_error_detail()->set_code(::grpc::INTERNAL);
  response.mutable_error_detail()->set_message("simulated Nighthawk Service error response");
  return response;
}

/**
 * Sets up a mock gRPC reader-writer to be returned from the mock Nighthawk Service stub. The mock
 * Read() return a non-converging Nighthawk response.
 *
 * @return Bare pointer that will be automatically wrapped in a unique_ptr by the caller.
 */
grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                      nighthawk::client::ExecutionResponse>*
MakeNonConvergingMockClientReaderWriter() {
  auto* mock_reader_writer =
      new grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                                nighthawk::client::ExecutionResponse>();
  EXPECT_CALL(*mock_reader_writer, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(MakeNighthawkResponseWithSendRate(0.5)), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_reader_writer, WritesDone()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_reader_writer, Finish()).WillRepeatedly(Return(::grpc::Status::OK));
  return mock_reader_writer;
}

/**
 * Sets up a mock gRPC reader-writer to be returned from the mock Nighthawk Service stub. The mock
 * Read() return a converging Nighthawk response.
 *
 * @return Bare pointer that will be automatically wrapped in a unique_ptr by the caller.
 */
grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                      nighthawk::client::ExecutionResponse>*
MakeConvergingMockClientReaderWriter() {
  auto* mock_reader_writer =
      new grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                                nighthawk::client::ExecutionResponse>();
  EXPECT_CALL(*mock_reader_writer, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(MakeNighthawkResponseWithSendRate(1.0)), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_reader_writer, WritesDone()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_reader_writer, Finish()).WillRepeatedly(Return(::grpc::Status::OK));
  return mock_reader_writer;
}

/**
 * Sets up a mock gRPC reader-writer to be returned from the mock Nighthawk Service stub. The mock
 * Read() return a failed Nighthawk response.
 *
 * @return Bare pointer that will be automatically wrapped in a unique_ptr by the caller.
 */
grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                      nighthawk::client::ExecutionResponse>*
MakeDoomedMockClientReaderWriter() {
  auto* mock_reader_writer =
      new grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                                nighthawk::client::ExecutionResponse>();
  EXPECT_CALL(*mock_reader_writer, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(MakeFailedNighthawkResponse()), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_reader_writer, WritesDone()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_reader_writer, Finish()).WillRepeatedly(Return(::grpc::Status::OK));
  return mock_reader_writer;
}

TEST(AdaptiveLoadController, FailsWithTrafficTemplateDurationSet) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template()->mutable_duration()->set_seconds(1);
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("should not have |duration| set"));
}

TEST(AdaptiveLoadController, FailsWithOpenLoopSet) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template()->mutable_open_loop()->set_value(false);
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("should not have |open_loop| set"));
}

TEST(AdaptiveLoadController, FailsWithNonexistentMetricsPluginName) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  envoy::config::core::v3::TypedExtensionConfig* metrics_plugin_config =
      spec.mutable_metrics_plugin_configs()->Add();
  metrics_plugin_config->set_name("nonexistent-plugin");
  *metrics_plugin_config->mutable_typed_config() = Envoy::ProtobufWkt::Any();
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("Failed to load MetricsPlugin"));
}

TEST(AdaptiveLoadController, FailsWithNonexistentStepControllerPluginName) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  envoy::config::core::v3::TypedExtensionConfig config;
  config.set_name("nonexistent-plugin");
  *config.mutable_typed_config() = Envoy::ProtobufWkt::Any();
  *spec.mutable_step_controller_config() = config;
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("Failed to load StepController"));
}

TEST(AdaptiveLoadController, FailsWithNonexistentScoringFunctionPluginName) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  envoy::config::core::v3::TypedExtensionConfig scoring_function_config;
  scoring_function_config.set_name("nonexistent-scoring-function");
  *scoring_function_config.mutable_typed_config() = Envoy::ProtobufWkt::Any();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() = scoring_function_config;
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("Failed to load ScoringFunction"));
}

TEST(AdaptiveLoadController, FailsWithNonexistentMetricsPluginNameInMetricThresholdSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  threshold->mutable_metric_spec()->set_metric_name("x");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nonexistent-metrics-plugin");
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(AdaptiveLoadController, FailsWithUndeclaredMetricsPluginNameInMetricThresholdSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  threshold->mutable_metric_spec()->set_metric_name("x");
  // Valid plugin name, but plugin not declared in the spec.
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(AdaptiveLoadController, FailsWithNonexistentMetricsPluginNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("x");
  metric_spec->set_metrics_plugin_name("nonexistent-metrics-plugin");
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(AdaptiveLoadController, FailsWithUndeclaredMetricsPluginNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("x");
  // Valid plugin name, but plugin not declared in the spec.
  metric_spec->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(AdaptiveLoadController, FailsWithNonexistentBuiltinMetricNameInMetricThresholdSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  threshold->mutable_metric_spec()->set_metric_name("nonexistent-metric-name");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.builtin");
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("not implemented by plugin"));
}

TEST(AdaptiveLoadController, FailsWithNonexistentCustomMetricNameInMetricThresholdSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  threshold->mutable_metric_spec()->set_metric_name("nonexistent-metric-name");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("not implemented by plugin"));
}

TEST(AdaptiveLoadController, FailsWithNonexistentBuiltinMetricNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("nonexistent-metric-name");
  metric_spec->set_metrics_plugin_name("nighthawk.builtin");
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("not implemented by plugin"));
}

TEST(AdaptiveLoadController, FailsWithNonexistentCustomMetricNameInInformationalMetricSpec) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("nonexistent-metric-name");
  metric_spec->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = PerformAdaptiveLoadSession(
      /*nighthawk_service_stub=*/nullptr, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("not implemented by plugin"));
}

TEST(AdaptiveLoadController, TimesOutIfNeverConverged) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeNonConvergingMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kDeadlineExceeded);
  EXPECT_THAT(output_or.status().message(), HasSubstr("Failed to converge before deadline"));
}

TEST(AdaptiveLoadController, UsesDefaultConvergenceDeadline) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  // Not setting convergence deadline, should default to 300 seconds.
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeNonConvergingMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  Envoy::MonotonicTime start_time = time_source.monotonicTime();
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  EXPECT_THAT(
      std::chrono::duration_cast<std::chrono::seconds>(time_source.monotonicTime() - start_time)
          .count(),
      303); // 300 ticks plus 2 monotonicTime() calls here plus 1 within the controller when it
            // recorded the start time.
}

TEST(AdaptiveLoadController, SetsOpenLoopMode) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  // Successively overwritten by each adjusting stage request and finally the testing stage request.
  nighthawk::client::ExecutionRequest request;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&request](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeConvergingMockClientReaderWriter();
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillRepeatedly(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  EXPECT_TRUE(request.start_request().options().open_loop().value());
}

TEST(AdaptiveLoadController, UsesConfiguredConvergenceDeadline) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();
  spec.mutable_convergence_deadline()->set_seconds(123);

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeNonConvergingMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  Envoy::MonotonicTime start_time = time_source.monotonicTime();
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  EXPECT_THAT(
      std::chrono::duration_cast<std::chrono::seconds>(time_source.monotonicTime() - start_time)
          .count(),
      126); // 123 ticks plus 2 monotonicTime() calls here plus 1 within the controller when it
            // recorded the start time.
}

TEST(AdaptiveLoadController, UsesDefaultTestingStageDuration) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  // Successively overwritten by each adjusting stage request and finally the testing stage request.
  nighthawk::client::ExecutionRequest request;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  int convergence_countdown = 5;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&request, &convergence_countdown](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = convergence_countdown > 0
                                       ? MakeNonConvergingMockClientReaderWriter()
                                       : MakeConvergingMockClientReaderWriter();
        --convergence_countdown;
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillRepeatedly(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  ASSERT_TRUE(output.has_testing_stage_result());
  EXPECT_EQ(request.start_request().options().duration().seconds(), 30);
}

TEST(AdaptiveLoadController, UsesDefaultMeasuringPeriod) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::client::ExecutionRequest request;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  int convergence_countdown = 5;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&request, &convergence_countdown](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = convergence_countdown > 0
                                       ? MakeNonConvergingMockClientReaderWriter()
                                       : MakeConvergingMockClientReaderWriter();
        --convergence_countdown;
        if (convergence_countdown > 0) {
          // Ensure we only capture the request if it's the adjusting stage.
          EXPECT_CALL(*mock_reader_writer, Write(_, _))
              .WillRepeatedly(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        }
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  EXPECT_EQ(request.start_request().options().duration().seconds(), 10);
}

TEST(AdaptiveLoadController, UsesConfiguredMeasuringPeriod) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();
  spec.mutable_measuring_period()->set_seconds(17);

  // Values to trigger convergence are not set up in the mock Nighthawk Service.

  nighthawk::client::ExecutionRequest request;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  int convergence_countdown = 5;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&request, &convergence_countdown](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = convergence_countdown > 0
                                       ? MakeNonConvergingMockClientReaderWriter()
                                       : MakeConvergingMockClientReaderWriter();
        --convergence_countdown;
        if (convergence_countdown > 0) {
          // Ensure we only capture the request if it's the adjusting stage.
          EXPECT_CALL(*mock_reader_writer, Write(_, _))
              .WillRepeatedly(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        }
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  EXPECT_EQ(request.start_request().options().duration().seconds(), 17);
}

TEST(AdaptiveLoadController, UsesCommandLineOptionsFromController) {
  // Always sends 1024 rps.
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::client::ExecutionRequest request;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&request](grpc_impl::ClientContext*) {
        auto* mock_reader_writer = MakeConvergingMockClientReaderWriter();
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillRepeatedly(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        return mock_reader_writer;
      });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  EXPECT_EQ(request.start_request().options().requests_per_second().value(), 1024);
}

TEST(AdaptiveLoadController, UsesDefaultMetricWeight) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("good_metric");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 1);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[1].weight(), 1.0);
}

TEST(AdaptiveLoadController, UsesCustomMetricWeight) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("good_metric");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.0);
  threshold->mutable_threshold_spec()->mutable_weight()->set_value(45.0);

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 1);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[1].weight(), 45.0);
}

TEST(AdaptiveLoadController, ExitsWhenDoomed) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  int doom_countdown = 3;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([&doom_countdown](grpc_impl::ClientContext*) {
        if (doom_countdown > 0) {
          --doom_countdown;
          return MakeNonConvergingMockClientReaderWriter();
        } else {
          return MakeDoomedMockClientReaderWriter();
        }
      });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_FALSE(output_or.ok());

  EXPECT_THAT(output_or.status().message(),
              HasSubstr("Step controller determined that it can never converge"));
  EXPECT_THAT(output_or.status().message(),
              HasSubstr("simulated Nighthawk Service error response"));
}

TEST(AdaptiveLoadController, PerformsTestingStageAfterConvergence) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  EXPECT_TRUE(output.has_testing_stage_result());
}

TEST(AdaptiveLoadController, SetsBenchmarkErrorStatusIfNighthawkServiceDoesNotSendResponse) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                                      nighthawk::client::ExecutionResponse>();
        // Simulate gRPC Read() failing:
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillRepeatedly(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish()).WillRepeatedly(Return(::grpc::Status::OK));
        return mock_reader_writer;
      });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kUnknown);
  EXPECT_THAT(output_or.status().message(),
              HasSubstr("Nighthawk Service did not send a gRPC response."));
}

TEST(AdaptiveLoadController, SetsBenchmarkErrorStatusIfNighthawkServiceWriteFails) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                                      nighthawk::client::ExecutionResponse>();
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillRepeatedly(Return(false));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish()).WillRepeatedly(Return(::grpc::Status::OK));
        return mock_reader_writer;
      });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kUnknown);
  EXPECT_THAT(output_or.status().message(), HasSubstr("Failed to write"));
}

TEST(AdaptiveLoadController, SetsBenchmarkErrorStatusIfNighthawkServiceWritesDoneFails) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                                      nighthawk::client::ExecutionResponse>();
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillRepeatedly(Return(false));
        EXPECT_CALL(*mock_reader_writer, Finish()).WillRepeatedly(Return(::grpc::Status::OK));
        return mock_reader_writer;
      });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kUnknown);
  EXPECT_THAT(output_or.status().message(), HasSubstr("WritesDone() failed"));
}

TEST(AdaptiveLoadController, SetsBenchmarkErrorStatusIfNighthawkServiceGrpcStreamClosesAbnormally) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<nighthawk::client::ExecutionRequest,
                                                      nighthawk::client::ExecutionResponse>();
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillOnce(Return(true))
            .WillRepeatedly(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillRepeatedly(Return(true));
        // Simulate gRPC abnormal stream shutdown:
        EXPECT_CALL(*mock_reader_writer, Finish())
            .WillRepeatedly(
                Return(::grpc::Status(::grpc::UNKNOWN, "Finish failure status message")));
        return mock_reader_writer;
      });
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kUnknown);
  EXPECT_THAT(output_or.status().message(), HasSubstr("Finish failure status message"));
}

TEST(AdaptiveLoadController, UsesBuiltinMetricsPluginForThresholdByDefault) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("success-rate");
  // metrics_plugin_name not set, defaults to nighthawk.builtin
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.9);

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 1);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[1].metric_value(), 0.03125);
}

TEST(AdaptiveLoadController, EvaluatesBuiltinMetric) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("success-rate");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.builtin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(0.9);

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 1);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[1].metric_value(), 0.03125);
  // Requested a lower threshold of 0.9 but only achieved 0.03125.
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[1].threshold_score(), -1.0);
}

TEST(AdaptiveLoadController, UsesBuiltinMetricsPluginForInformationalMetricSpecByDefault) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("success-rate");
  // metrics_plugin_name not set, defaults to nighthawk.builtin

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 1);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[1].metric_value(), 0.03125);
}

TEST(AdaptiveLoadController, StoresInformationalBuiltinMetric) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("success-rate");
  metric_spec->set_metrics_plugin_name("nighthawk.builtin");

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 1);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[1].metric_value(), 0.03125);
}

TEST(AdaptiveLoadController, EvaluatesCustomMetric) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();
  // Configures a metric with value 5.0:
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();
  nighthawk::adaptive_load::MetricSpecWithThreshold* threshold =
      spec.mutable_metric_thresholds()->Add();
  threshold->mutable_metric_spec()->set_metric_name("good_metric");
  threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");
  *threshold->mutable_threshold_spec()->mutable_scoring_function() =
      MakeLowerThresholdBinaryScoringFunctionConfig(6.0);

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 1);
  // Requested a lower threshold of 6.0 but only achieved 5.0.
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[1].threshold_score(), -1.0);
}

TEST(AdaptiveLoadController, StoresInformationalCustomMetric) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("good_metric");
  metric_spec->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  ASSERT_GT(output.adjusting_stage_results_size(), 0);
  ASSERT_GT(output.adjusting_stage_results()[0].metric_evaluations_size(), 1);
  EXPECT_EQ(output.adjusting_stage_results()[0].metric_evaluations()[1].metric_value(), 5.0);
}

TEST(AdaptiveLoadController, PropagatesErrorFromMetricsPlugin) {
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("bad_metric");
  metric_spec->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_THAT(output_or.status().message(), HasSubstr("Error calling MetricsPlugin"));
}

TEST(AdaptiveLoadController, CopiesThresholdSpecToOutput) {
  // Spec contains a threshold for send-rate:
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });
  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_TRUE(output_or.ok());
  AdaptiveLoadSessionOutput output = output_or.value();
  ASSERT_GT(output.metric_thresholds_size(), 0);
  EXPECT_EQ(output.metric_thresholds()[0].metric_spec().metric_name(), "send-rate");
}

TEST(AdaptiveLoadController, PropagatesInputVariableSettingError) {
  const std::string kExpectedErrorMessage = "artificial input value setting error";
  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec = MakeConvergeableDoomableSessionSpec();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerPluginConfigWithInputSettingError(
      absl::PermissionDeniedError(kExpectedErrorMessage));

  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginConfig();
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("good_metric");
  metric_spec->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");

  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillRepeatedly(
          [](grpc_impl::ClientContext*) { return MakeConvergingMockClientReaderWriter(); });

  FakeIncrementingMonotonicTimeSource time_source;
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      PerformAdaptiveLoadSession(&mock_nighthawk_service_stub, spec, time_source);
  ASSERT_FALSE(output_or.ok());
  EXPECT_THAT(output_or.status().message(), HasSubstr(kExpectedErrorMessage));
}

} // namespace

} // namespace Nighthawk
