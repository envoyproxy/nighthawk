#include <chrono>

#include "envoy/config/core/v3/base.pb.h"
#include "envoy/registry/registry.h"

#include "nighthawk/adaptive_load/adaptive_load_controller.h"
#include "nighthawk/adaptive_load/input_variable_setter.h"
#include "nighthawk/adaptive_load/metrics_evaluator.h"
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

#include "source/adaptive_load/adaptive_load_controller_impl.h"
#include "source/adaptive_load/metrics_plugin_impl.h"
#include "source/adaptive_load/plugin_loader.h"
#include "source/adaptive_load/scoring_function_impl.h"
#include "source/adaptive_load/session_spec_proto_helper_impl.h"

#include "test/adaptive_load/fake_plugins/fake_step_controller/fake_step_controller.h"
#include "test/common/fake_time_source.h"
#include "test/mocks/adaptive_load/mock_metrics_evaluator.h"
#include "test/mocks/adaptive_load/mock_session_spec_proto_helper.h"
#include "test/mocks/common/mock_nighthawk_service_client.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

using ::Envoy::Protobuf::util::MessageDifferencer;
using ::nighthawk::adaptive_load::AdaptiveLoadSessionOutput;
using ::nighthawk::adaptive_load::AdaptiveLoadSessionSpec;
using ::nighthawk::adaptive_load::BenchmarkResult;
using ::nighthawk::adaptive_load::MetricEvaluation;
using ::nighthawk::adaptive_load::MetricSpec;
using ::nighthawk::adaptive_load::MetricSpecWithThreshold;
using ::nighthawk::adaptive_load::ThresholdSpec;
using ::nighthawk::client::MockNighthawkServiceStub;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;

// The start time used in these tests.
const int kFakeStartTimeSeconds = 10;

/**
 * Creates a valid BenchmarkResult proto with only the score set. Useful for controlling the
 * FakeStepController, which returns convergence for score > 0 and doom for a score < 0.
 *
 * @param score Positive number for a converging BenchmarkResult, negative number for doomed, zero
 * for neither.
 *
 * @return BenchmarkResult An incomplete BenchmarkResult useful only for determining
 * FakeStepController convergence and doom.
 */
BenchmarkResult MakeBenchmarkResultWithScore(double score) {
  BenchmarkResult benchmark_result;
  MetricEvaluation* evaluation = benchmark_result.mutable_metric_evaluations()->Add();
  evaluation->set_threshold_score(score);
  return benchmark_result;
}

/**
 * Creates a minimal AdaptiveLoadSessionSpec with a FakeStepController.
 *
 * @return AdaptiveLoadSessionSpec with a FakeStepController and enough fields set to pass
 * validation.
 */
AdaptiveLoadSessionSpec MakeValidAdaptiveLoadSessionSpec() {
  AdaptiveLoadSessionSpec spec;
  spec.mutable_convergence_deadline()->set_seconds(100);
  *spec.mutable_step_controller_config() = MakeFakeStepControllerPluginConfigWithRps(10);
  MetricSpecWithThreshold* expected_spec_with_threshold = spec.mutable_metric_thresholds()->Add();
  expected_spec_with_threshold->mutable_metric_spec()->set_metric_name("success-rate");
  expected_spec_with_threshold->mutable_threshold_spec()->mutable_scoring_function()->set_name(
      "nighthawk.binary_scoring");
  expected_spec_with_threshold->mutable_threshold_spec()
      ->mutable_scoring_function()
      ->mutable_typed_config()
      ->PackFrom(nighthawk::adaptive_load::BinaryScoringFunctionConfig());
  return spec;
}

class AdaptiveLoadControllerImplFixture : public testing::Test {
public:
  void SetUp() override {
    fake_time_source_.setSystemTimeSeconds(kFakeStartTimeSeconds);

    ON_CALL(mock_nighthawk_service_client_, PerformNighthawkBenchmark)
        .WillByDefault(Return(nighthawk::client::ExecutionResponse()));
  }

protected:
  NiceMock<MockNighthawkServiceClient> mock_nighthawk_service_client_;
  NiceMock<MockMetricsEvaluator> mock_metrics_evaluator_;
  FakeIncrementingTimeSource fake_time_source_;
  MockNighthawkServiceStub mock_nighthawk_service_stub_;
  // Real spec helper is simpler to use because SetSessionSpecDefaults preserves values a test
  // sets in the spec; the mock inconveniently discards the input and returns an empty spec.
  AdaptiveLoadSessionSpecProtoHelperImpl real_spec_proto_helper_;
};

TEST_F(AdaptiveLoadControllerImplFixture, SetsSpecDefaults) {
  NiceMock<MockAdaptiveLoadSessionSpecProtoHelper> mock_spec_proto_helper;
  AdaptiveLoadSessionSpec spec = MakeValidAdaptiveLoadSessionSpec();
  EXPECT_CALL(mock_spec_proto_helper, SetSessionSpecDefaults(_)).WillOnce(Return(spec));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        mock_spec_proto_helper, fake_time_source_);

  (void)controller.PerformAdaptiveLoadSession(&mock_nighthawk_service_stub_, spec);
}

TEST_F(AdaptiveLoadControllerImplFixture, PropagatesSpecValidationError) {
  NiceMock<MockAdaptiveLoadSessionSpecProtoHelper> mock_spec_proto_helper;
  EXPECT_CALL(mock_spec_proto_helper, CheckSessionSpec(_))
      .WillOnce(Return(absl::DataLossError("artificial spec error")));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        mock_spec_proto_helper, fake_time_source_);

  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = controller.PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub_, MakeValidAdaptiveLoadSessionSpec());
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kDataLoss);
  EXPECT_EQ(output_or.status().message(), "artificial spec error");
}

TEST_F(AdaptiveLoadControllerImplFixture, CopiesThresholdSpecsIntoOutput) {
  EXPECT_CALL(mock_metrics_evaluator_, AnalyzeNighthawkBenchmark(_, _, _))
      .WillRepeatedly(Return(MakeBenchmarkResultWithScore(1.0)));

  AdaptiveLoadSessionSpecProtoHelperImpl spec_helper;
  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        spec_helper, fake_time_source_);

  AdaptiveLoadSessionSpec spec =
      spec_helper.SetSessionSpecDefaults(MakeValidAdaptiveLoadSessionSpec());
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      controller.PerformAdaptiveLoadSession(&mock_nighthawk_service_stub_, spec);
  ASSERT_TRUE(output_or.ok());
  ASSERT_GT(output_or.value().metric_thresholds_size(), 0);
  MetricSpecWithThreshold actual_spec_with_threshold = output_or.value().metric_thresholds(0);
  EXPECT_TRUE(
      MessageDifferencer::Equivalent(actual_spec_with_threshold, spec.metric_thresholds(0)));
  EXPECT_EQ(actual_spec_with_threshold.DebugString(), spec.metric_thresholds(0).DebugString());
}

TEST_F(AdaptiveLoadControllerImplFixture, TimesOutIfNeverConverged) {
  EXPECT_CALL(mock_metrics_evaluator_, AnalyzeNighthawkBenchmark(_, _, _))
      .WillRepeatedly(Return(MakeBenchmarkResultWithScore(0.0)));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        real_spec_proto_helper_, fake_time_source_);

  AdaptiveLoadSessionSpec spec = MakeValidAdaptiveLoadSessionSpec();
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      controller.PerformAdaptiveLoadSession(&mock_nighthawk_service_stub_, spec);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kDeadlineExceeded);
  EXPECT_THAT(output_or.status().message(), HasSubstr("Failed to converge"));
}

TEST_F(AdaptiveLoadControllerImplFixture, ReturnsErrorWhenDoomed) {
  EXPECT_CALL(mock_metrics_evaluator_, AnalyzeNighthawkBenchmark(_, _, _))
      .WillOnce(Return(MakeBenchmarkResultWithScore(-1.0)));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        real_spec_proto_helper_, fake_time_source_);

  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = controller.PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub_, MakeValidAdaptiveLoadSessionSpec());
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kAborted);
  EXPECT_THAT(output_or.status().message(), HasSubstr("can never converge"));
}

TEST_F(AdaptiveLoadControllerImplFixture,
       PropagatesErrorWhenInputValueSettingFailsInAdjustingStage) {
  const std::string kExpectedErrorMessage = "artificial input setting error";
  EXPECT_CALL(mock_metrics_evaluator_, AnalyzeNighthawkBenchmark(_, _, _))
      .WillRepeatedly(Return(MakeBenchmarkResultWithScore(-1.0)));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        real_spec_proto_helper_, fake_time_source_);

  AdaptiveLoadSessionSpec spec = MakeValidAdaptiveLoadSessionSpec();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerPluginConfigWithInputSettingError(
      10, absl::DataLossError(kExpectedErrorMessage), /*countdown=*/0);
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      controller.PerformAdaptiveLoadSession(&mock_nighthawk_service_stub_, spec);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kDataLoss);
  EXPECT_THAT(output_or.status().message(), HasSubstr(kExpectedErrorMessage));
}

TEST_F(AdaptiveLoadControllerImplFixture, PropagatesErrorWhenInputValueSettingFailsInTestingStage) {
  const std::string kExpectedErrorMessage = "artificial input setting error";
  EXPECT_CALL(mock_metrics_evaluator_, AnalyzeNighthawkBenchmark(_, _, _))
      .WillRepeatedly(Return(MakeBenchmarkResultWithScore(1.0)));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        real_spec_proto_helper_, fake_time_source_);

  AdaptiveLoadSessionSpec spec = MakeValidAdaptiveLoadSessionSpec();
  *spec.mutable_step_controller_config() = MakeFakeStepControllerPluginConfigWithInputSettingError(
      10, absl::DataLossError(kExpectedErrorMessage), /*countdown=*/1);
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      controller.PerformAdaptiveLoadSession(&mock_nighthawk_service_stub_, spec);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kDataLoss);
  EXPECT_THAT(output_or.status().message(), HasSubstr(kExpectedErrorMessage));
}

TEST_F(AdaptiveLoadControllerImplFixture, PropagatesErrorFromNighthawkService) {
  const std::string kExpectedErrorMessage = "artificial nighthawk service error";
  EXPECT_CALL(mock_nighthawk_service_client_, PerformNighthawkBenchmark(_, _))
      .WillOnce(Return(absl::DataLossError(kExpectedErrorMessage)));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        real_spec_proto_helper_, fake_time_source_);

  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = controller.PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub_, MakeValidAdaptiveLoadSessionSpec());
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kDataLoss);
  EXPECT_THAT(output_or.status().message(), HasSubstr(kExpectedErrorMessage));
}

TEST_F(AdaptiveLoadControllerImplFixture, PropagatesErrorFromMetricsEvaluator) {
  const std::string kExpectedErrorMessage = "artificial metrics evaluator error";
  EXPECT_CALL(mock_metrics_evaluator_, AnalyzeNighthawkBenchmark(_, _, _))
      .WillOnce(Return(absl::DataLossError(kExpectedErrorMessage)));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        real_spec_proto_helper_, fake_time_source_);

  absl::StatusOr<AdaptiveLoadSessionOutput> output_or = controller.PerformAdaptiveLoadSession(
      &mock_nighthawk_service_stub_, MakeValidAdaptiveLoadSessionSpec());
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kDataLoss);
  EXPECT_THAT(output_or.status().message(), HasSubstr(kExpectedErrorMessage));
}

TEST_F(AdaptiveLoadControllerImplFixture, StoresAdjustingStageResult) {
  BenchmarkResult expected_benchmark_result = MakeBenchmarkResultWithScore(1.0);
  expected_benchmark_result.mutable_start_time()->set_seconds(kFakeStartTimeSeconds);
  expected_benchmark_result.mutable_end_time()->set_seconds(kFakeStartTimeSeconds + 1);

  EXPECT_CALL(mock_metrics_evaluator_, AnalyzeNighthawkBenchmark(_, _, _))
      .WillRepeatedly(Return(expected_benchmark_result));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        real_spec_proto_helper_, fake_time_source_);

  AdaptiveLoadSessionSpec spec = MakeValidAdaptiveLoadSessionSpec();
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      controller.PerformAdaptiveLoadSession(&mock_nighthawk_service_stub_, spec);
  ASSERT_TRUE(output_or.ok());
  ASSERT_EQ(output_or.value().adjusting_stage_results_size(), 1);
  const BenchmarkResult& actual_benchmark_result = output_or.value().adjusting_stage_results(0);
  EXPECT_TRUE(MessageDifferencer::Equivalent(actual_benchmark_result, expected_benchmark_result));
  EXPECT_EQ(actual_benchmark_result.DebugString(), expected_benchmark_result.DebugString());
}

TEST_F(AdaptiveLoadControllerImplFixture, StoresTestingStageResult) {
  BenchmarkResult expected_benchmark_result = MakeBenchmarkResultWithScore(1.0);
  // Times kFakeStartTimeSeconds and kFakeStartTimeSeconds + 1 are taken by the
  // adjusting stage.
  expected_benchmark_result.mutable_start_time()->set_seconds(kFakeStartTimeSeconds + 2);
  expected_benchmark_result.mutable_end_time()->set_seconds(kFakeStartTimeSeconds + 3);

  EXPECT_CALL(mock_metrics_evaluator_, AnalyzeNighthawkBenchmark(_, _, _))
      .WillRepeatedly(Return(expected_benchmark_result));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        real_spec_proto_helper_, fake_time_source_);

  AdaptiveLoadSessionSpec spec = MakeValidAdaptiveLoadSessionSpec();
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      controller.PerformAdaptiveLoadSession(&mock_nighthawk_service_stub_, spec);
  ASSERT_TRUE(output_or.ok());
  const BenchmarkResult& actual_benchmark_result = output_or.value().testing_stage_result();
  EXPECT_TRUE(MessageDifferencer::Equivalent(actual_benchmark_result, expected_benchmark_result));
  EXPECT_EQ(actual_benchmark_result.DebugString(), expected_benchmark_result.DebugString());
}

TEST_F(AdaptiveLoadControllerImplFixture, SucceedsWhenBenchmarkCooldownRequested) {
  BenchmarkResult expected_benchmark_result = MakeBenchmarkResultWithScore(1.0);
  EXPECT_CALL(mock_metrics_evaluator_, AnalyzeNighthawkBenchmark(_, _, _))
      .WillRepeatedly(Return(expected_benchmark_result));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        real_spec_proto_helper_, fake_time_source_);

  AdaptiveLoadSessionSpec spec = MakeValidAdaptiveLoadSessionSpec();
  spec.mutable_benchmark_cooldown_duration()->set_nanos(10);
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      controller.PerformAdaptiveLoadSession(&mock_nighthawk_service_stub_, spec);
  EXPECT_TRUE(output_or.ok());
}

TEST_F(AdaptiveLoadControllerImplFixture, FailsWhenBenchmarkCooldownDurationIsNegative) {
  BenchmarkResult expected_benchmark_result = MakeBenchmarkResultWithScore(1.0);
  EXPECT_CALL(mock_metrics_evaluator_, AnalyzeNighthawkBenchmark(_, _, _))
      .WillRepeatedly(Return(expected_benchmark_result));

  AdaptiveLoadControllerImpl controller(mock_nighthawk_service_client_, mock_metrics_evaluator_,
                                        real_spec_proto_helper_, fake_time_source_);

  AdaptiveLoadSessionSpec spec = MakeValidAdaptiveLoadSessionSpec();
  spec.mutable_benchmark_cooldown_duration()->set_nanos(-10);
  absl::StatusOr<AdaptiveLoadSessionOutput> output_or =
      controller.PerformAdaptiveLoadSession(&mock_nighthawk_service_stub_, spec);
  ASSERT_FALSE(output_or.ok());
  EXPECT_EQ(output_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(output_or.status().message(), HasSubstr("BenchmarkCooldownDuration"));
}

} // namespace

} // namespace Nighthawk
