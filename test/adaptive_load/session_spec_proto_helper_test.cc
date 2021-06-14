#include "envoy/registry/registry.h"

#include "external/envoy/source/common/config/utility.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/metric_spec.pb.h"
#include "api/client/options.pb.h"

#include "source/adaptive_load/plugin_loader.h"
#include "source/adaptive_load/session_spec_proto_helper_impl.h"

#include "test/adaptive_load/fake_plugins/fake_metrics_plugin/fake_metrics_plugin.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

using ::nighthawk::adaptive_load::AdaptiveLoadSessionSpec;
using ::nighthawk::adaptive_load::MetricSpecWithThreshold;
using ::testing::HasSubstr;

TEST(SetSessionSpecDefaults, SetsDefaultValueIfOpenLoopUnset) {
  AdaptiveLoadSessionSpec original_spec;
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  EXPECT_TRUE(spec.nighthawk_traffic_template().open_loop().value());
}

TEST(SetSessionSpecDefaults, PreservesExplicitOpenLoopSetting) {
  AdaptiveLoadSessionSpec original_spec;
  original_spec.mutable_nighthawk_traffic_template()->mutable_open_loop()->set_value(false);
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  EXPECT_FALSE(spec.nighthawk_traffic_template().open_loop().value());
}

TEST(SetSessionSpecDefaults, SetsDefaultMeasuringPeriodIfUnset) {
  AdaptiveLoadSessionSpec original_spec;
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  EXPECT_EQ(spec.measuring_period().seconds(), 10);
}

TEST(SetSessionSpecDefaults, PreservesExplicitMeasuringPeriod) {
  const int kExpectedMeasuringPeriodSeconds = 123;
  AdaptiveLoadSessionSpec original_spec;
  original_spec.mutable_measuring_period()->set_seconds(kExpectedMeasuringPeriodSeconds);
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  EXPECT_EQ(spec.measuring_period().seconds(), kExpectedMeasuringPeriodSeconds);
}

TEST(SetSessionSpecDefaults, SetsDefaultConvergenceDeadlineIfUnset) {
  AdaptiveLoadSessionSpec original_spec;
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  EXPECT_EQ(spec.convergence_deadline().seconds(), 300);
}

TEST(SetSessionSpecDefaults, PreservesExplicitConvergenceDeadline) {
  const int kExpectedConvergenceDeadlineSeconds = 123;
  AdaptiveLoadSessionSpec original_spec;
  original_spec.mutable_convergence_deadline()->set_seconds(kExpectedConvergenceDeadlineSeconds);
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  EXPECT_EQ(spec.convergence_deadline().seconds(), kExpectedConvergenceDeadlineSeconds);
}

TEST(SetSessionSpecDefaults, SetsDefaultTestingStageDurationIfUnset) {
  AdaptiveLoadSessionSpec original_spec;
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  EXPECT_EQ(spec.testing_stage_duration().seconds(), 30);
}

TEST(SetSessionSpecDefaults, PreservesExplicitTestingStageDuration) {
  const int kExpectedTestingStageDurationSeconds = 123;
  AdaptiveLoadSessionSpec original_spec;
  original_spec.mutable_testing_stage_duration()->set_seconds(kExpectedTestingStageDurationSeconds);
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  EXPECT_EQ(spec.testing_stage_duration().seconds(), kExpectedTestingStageDurationSeconds);
}

TEST(SetSessionSpecDefaults, SetsDefaultScoredMetricPluginNameIfUnset) {
  AdaptiveLoadSessionSpec original_spec;
  (void)original_spec.mutable_metric_thresholds()->Add();
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  ASSERT_GT(spec.metric_thresholds_size(), 0);
  EXPECT_EQ(spec.metric_thresholds(0).metric_spec().metrics_plugin_name(), "nighthawk.builtin");
}

TEST(SetSessionSpecDefaults, PreservesExplicitScoredMetricPluginName) {
  const std::string kExpectedMetricsPluginName = "a";
  AdaptiveLoadSessionSpec original_spec;
  MetricSpecWithThreshold* spec_threshold = original_spec.mutable_metric_thresholds()->Add();
  spec_threshold->mutable_metric_spec()->set_metrics_plugin_name(kExpectedMetricsPluginName);
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  ASSERT_GT(spec.metric_thresholds_size(), 0);
  EXPECT_EQ(spec.metric_thresholds(0).metric_spec().metrics_plugin_name(),
            kExpectedMetricsPluginName);
}

TEST(SetSessionSpecDefaults, SetsDefaultScoredMetricWeightIfUnset) {
  AdaptiveLoadSessionSpec original_spec;
  (void)original_spec.mutable_metric_thresholds()->Add();
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  ASSERT_GT(spec.metric_thresholds_size(), 0);
  EXPECT_EQ(spec.metric_thresholds(0).threshold_spec().weight().value(), 1.0);
}

TEST(SetSessionSpecDefaults, PreservesExplicitScoredMetricWeight) {
  const double kExpectedWeight = 123.0;
  AdaptiveLoadSessionSpec original_spec;
  MetricSpecWithThreshold* spec_threshold = original_spec.mutable_metric_thresholds()->Add();
  spec_threshold->mutable_threshold_spec()->mutable_weight()->set_value(kExpectedWeight);
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  ASSERT_GT(spec.metric_thresholds_size(), 0);
  EXPECT_EQ(spec.metric_thresholds(0).threshold_spec().weight().value(), kExpectedWeight);
}

TEST(SetSessionSpecDefaults, SetsDefaultInformationalMetricPluginNameIfUnset) {
  AdaptiveLoadSessionSpec original_spec;
  (void)original_spec.mutable_informational_metric_specs()->Add();
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  ASSERT_GT(spec.informational_metric_specs_size(), 0);
  EXPECT_EQ(spec.informational_metric_specs(0).metrics_plugin_name(), "nighthawk.builtin");
}

TEST(SetSessionSpecDefaults, PreservesExplicitInformationalMetricPluginName) {
  const std::string kExpectedMetricsPluginName = "a";
  AdaptiveLoadSessionSpec original_spec;
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      original_spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metrics_plugin_name(kExpectedMetricsPluginName);
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec = helper.SetSessionSpecDefaults(original_spec);
  ASSERT_GT(spec.informational_metric_specs_size(), 0);
  EXPECT_EQ(spec.informational_metric_specs(0).metrics_plugin_name(), kExpectedMetricsPluginName);
}

// Returns a load session spec that can pass PGV (protoc-gen-validate).
AdaptiveLoadSessionSpec MakeSpecThatPassesProtoValidation() {
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  AdaptiveLoadSessionSpec spec;

  spec.mutable_nighthawk_traffic_template();
  helper.SetSessionSpecDefaults(spec);
  MetricSpecWithThreshold* spec_threshold = spec.mutable_metric_thresholds()->Add();
  spec_threshold->mutable_metric_spec()->set_metric_name("bogus");
  spec_threshold->mutable_threshold_spec()->mutable_scoring_function()->set_name("bogus");
  spec_threshold->mutable_threshold_spec()->mutable_scoring_function()->mutable_typed_config();
  return spec;
}

TEST(CheckSessionSpec, RejectsMeasuringPeriodIfSecondsNegative) {
  AdaptiveLoadSessionSpec spec = MakeSpecThatPassesProtoValidation();
  spec.mutable_measuring_period()->set_seconds(-1);

  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("MeasuringPeriod"));
}

TEST(CheckSessionSpec, RejectsMeasuringPeriodIfNanosNegative) {
  AdaptiveLoadSessionSpec spec = MakeSpecThatPassesProtoValidation();
  spec.mutable_measuring_period()->set_nanos(-1);

  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("MeasuringPeriod"));
}

TEST(CheckSessionSpec, RejectsConvergenceDeadlineIfSecondsNegative) {
  AdaptiveLoadSessionSpec spec = MakeSpecThatPassesProtoValidation();
  spec.mutable_convergence_deadline()->set_seconds(-1);

  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("ConvergenceDeadline"));
}

TEST(CheckSessionSpec, RejectsConvergenceDeadlineIfNanosNegative) {
  AdaptiveLoadSessionSpec spec = MakeSpecThatPassesProtoValidation();
  spec.mutable_convergence_deadline()->set_nanos(-1);

  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("ConvergenceDeadline"));
}

TEST(CheckSessionSpec, RejectsTestingStageDurationIfSecondsNegative) {
  AdaptiveLoadSessionSpec spec = MakeSpecThatPassesProtoValidation();
  spec.mutable_testing_stage_duration()->set_seconds(-1);

  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("TestingStageDuration"));
}

TEST(CheckSessionSpec, RejectsTestingStageDurationIfNanosNegative) {
  AdaptiveLoadSessionSpec spec = MakeSpecThatPassesProtoValidation();
  spec.mutable_testing_stage_duration()->set_nanos(-1);

  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("TestingStageDuration"));
}

TEST(CheckSessionSpec, RejectsBenchmarkCooldownDurationIfSecondsNegative) {
  AdaptiveLoadSessionSpec spec = MakeSpecThatPassesProtoValidation();
  spec.mutable_benchmark_cooldown_duration()->set_seconds(-1);

  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("BenchmarkCooldownDuration"));
}

TEST(CheckSessionSpec, RejectsBenchmarkCooldownDurationIfNanosNegative) {
  AdaptiveLoadSessionSpec spec = MakeSpecThatPassesProtoValidation();
  spec.mutable_benchmark_cooldown_duration()->set_nanos(-1);

  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("BenchmarkCooldownDuration"));
}

TEST(CheckSessionSpec, RejectsDurationIfSet) {
  AdaptiveLoadSessionSpec spec;
  spec.mutable_nighthawk_traffic_template()->mutable_duration()->set_seconds(1);
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("should not have |duration| set"));
}

TEST(CheckSessionSpec, RejectsInvalidMetricsPlugin) {
  AdaptiveLoadSessionSpec spec;
  envoy::config::core::v3::TypedExtensionConfig metrics_plugin_config;
  metrics_plugin_config.set_name("bogus");
  *spec.mutable_metrics_plugin_configs()->Add() = metrics_plugin_config;
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("Failed to load MetricsPlugin"));
}

TEST(CheckSessionSpec, RejectsInvalidStepControllerPlugin) {
  AdaptiveLoadSessionSpec spec;
  spec.mutable_step_controller_config()->set_name("bogus");
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("Failed to load StepController plugin"));
}

TEST(CheckSessionSpec, RejectsInvalidScoringFunctionPlugin) {
  AdaptiveLoadSessionSpec spec;
  MetricSpecWithThreshold* spec_threshold = spec.mutable_metric_thresholds()->Add();
  spec_threshold->mutable_threshold_spec()->mutable_scoring_function()->set_name("bogus");
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("Failed to load ScoringFunction plugin"));
}

TEST(CheckSessionSpec, RejectsScoredMetricWithUndeclaredMetricsPluginName) {
  AdaptiveLoadSessionSpec spec;
  MetricSpecWithThreshold* spec_threshold = spec.mutable_metric_thresholds()->Add();
  spec_threshold->mutable_metric_spec()->set_metrics_plugin_name("bogus");
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(CheckSessionSpec, RejectsInformationalMetricWithUndeclaredMetricsPluginName) {
  AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metrics_plugin_name("bogus");
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("nonexistent metrics_plugin_name"));
}

TEST(CheckSessionSpec, RejectsScoredMetricWithNonexistentDefaultMetricsPluginMetric) {
  AdaptiveLoadSessionSpec spec;
  MetricSpecWithThreshold* spec_threshold = spec.mutable_metric_thresholds()->Add();
  spec_threshold->mutable_metric_spec()->set_metric_name("bogus");
  spec_threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.builtin");
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("not implemented by plugin"));
}

TEST(CheckSessionSpec, RejectsInformationalMetricWithNonexistentDefaultMetricsPluginMetric) {
  AdaptiveLoadSessionSpec spec;
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("bogus");
  metric_spec->set_metrics_plugin_name("nighthawk.builtin");
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("not implemented by plugin"));
}

TEST(CheckSessionSpec, RejectsScoredMetricWithNonexistentCustomMetricsPluginMetric) {
  AdaptiveLoadSessionSpec spec;
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginTypedExtensionConfig(
      nighthawk::adaptive_load::FakeMetricsPluginConfig());
  MetricSpecWithThreshold* spec_threshold = spec.mutable_metric_thresholds()->Add();
  spec_threshold->mutable_metric_spec()->set_metric_name("bogus");
  spec_threshold->mutable_metric_spec()->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("not implemented by plugin"));
}

TEST(CheckSessionSpec, RejectsInformationalMetricWithNonexistentCustomMetricsPluginMetric) {
  AdaptiveLoadSessionSpec spec;
  *spec.mutable_metrics_plugin_configs()->Add() = MakeFakeMetricsPluginTypedExtensionConfig(
      nighthawk::adaptive_load::FakeMetricsPluginConfig());
  nighthawk::adaptive_load::MetricSpec* metric_spec =
      spec.mutable_informational_metric_specs()->Add();
  metric_spec->set_metric_name("bogus");
  metric_spec->set_metrics_plugin_name("nighthawk.fake_metrics_plugin");
  AdaptiveLoadSessionSpecProtoHelperImpl helper;
  absl::Status status = helper.CheckSessionSpec(spec);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("not implemented by plugin"));
}

} // namespace
} // namespace Nighthawk
