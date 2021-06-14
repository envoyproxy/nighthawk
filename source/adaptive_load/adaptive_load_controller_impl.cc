#include "source/adaptive_load/adaptive_load_controller_impl.h"

#include <chrono>

#include "envoy/common/exception.h"
#include "envoy/config/core/v3/base.pb.h"

#include "nighthawk/adaptive_load/adaptive_load_controller.h"
#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "nighthawk/adaptive_load/scoring_function.h"
#include "nighthawk/adaptive_load/step_controller.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/protobuf/protobuf.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/metric_spec.pb.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"
#include "api/client/service.grpc.pb.h"

#include "source/adaptive_load/metrics_plugin_impl.h"
#include "source/adaptive_load/plugin_loader.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

namespace Nighthawk {

namespace {

using nighthawk::adaptive_load::AdaptiveLoadSessionOutput;
using nighthawk::adaptive_load::AdaptiveLoadSessionSpec;
using nighthawk::adaptive_load::BenchmarkResult;
using nighthawk::adaptive_load::MetricEvaluation;
using nighthawk::adaptive_load::MetricSpec;
using nighthawk::adaptive_load::MetricSpecWithThreshold;
using nighthawk::adaptive_load::ThresholdSpec;

/**
 * Loads and initializes MetricsPlugins requested in the session spec. Assumes the spec has already
 * been validated; crashes the process otherwise.
 *
 * @param spec Adaptive load session spec proto that has already been validated.
 *
 * @return Map from MetricsPlugin names to initialized plugins, to be used in the course of a single
 * adaptive load session based on the session spec.
 */
absl::flat_hash_map<std::string, MetricsPluginPtr>
LoadMetricsPlugins(const AdaptiveLoadSessionSpec& spec) {
  absl::flat_hash_map<std::string, MetricsPluginPtr> name_to_custom_metrics_plugin_map;
  for (const envoy::config::core::v3::TypedExtensionConfig& config :
       spec.metrics_plugin_configs()) {
    absl::StatusOr<MetricsPluginPtr> metrics_plugin_or = LoadMetricsPlugin(config);
    RELEASE_ASSERT(
        metrics_plugin_or.ok(),
        absl::StrCat(
            "MetricsPlugin loading error should have been caught during input validation: ",
            metrics_plugin_or.status().message()));
    name_to_custom_metrics_plugin_map[config.name()] = std::move(metrics_plugin_or.value());
  }
  return name_to_custom_metrics_plugin_map;
}

/**
 * Logs the execution response excluding all non-global results and the
 * statistics from the global result.
 *
 * @param response is the execution response that should be logged.
 */
void LogGlobalResultExcludingStatistics(const nighthawk::client::ExecutionResponse& response) {
  nighthawk::client::ExecutionResponse stripped = response;
  stripped.mutable_output()->clear_results();
  for (const nighthawk::client::Result& result : response.output().results()) {
    if (result.name() != "global") {
      continue;
    }
    nighthawk::client::Result* stripped_result = stripped.mutable_output()->add_results();
    *stripped_result = result;
    stripped_result->clear_statistics();
  }
  ENVOY_LOG_MISC(info,
                 "Got result (stripped to just the global result excluding "
                 "statistics): {}",
                 stripped.DebugString());
}

/**
 * Loads and initializes a StepController plugin requested in the session spec. Assumes
 * the spec has already been validated; crashes the process otherwise.
 *
 * @param spec Adaptive load session spec proto that has already been validated.
 *
 * @return unique_ptr<StepController> Initialized StepController plugin.
 */
StepControllerPtr LoadStepControllerPluginFromSpec(const AdaptiveLoadSessionSpec& spec) {
  absl::StatusOr<StepControllerPtr> step_controller_or =
      LoadStepControllerPlugin(spec.step_controller_config(), spec.nighthawk_traffic_template());
  RELEASE_ASSERT(
      step_controller_or.ok(),
      absl::StrCat(
          "StepController plugin loading error should have been caught during input validation: ",
          step_controller_or.status().message()));
  return std::move(step_controller_or.value());
}

} // namespace

AdaptiveLoadControllerImpl::AdaptiveLoadControllerImpl(
    const NighthawkServiceClient& nighthawk_service_client,
    const MetricsEvaluator& metrics_evaluator,
    const AdaptiveLoadSessionSpecProtoHelper& session_spec_proto_helper,
    Envoy::TimeSource& time_source)
    : nighthawk_service_client_{nighthawk_service_client}, metrics_evaluator_{metrics_evaluator},
      session_spec_proto_helper_{session_spec_proto_helper}, time_source_{time_source} {}

absl::StatusOr<BenchmarkResult> AdaptiveLoadControllerImpl::PerformAndAnalyzeNighthawkBenchmark(
    nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
    const AdaptiveLoadSessionSpec& spec,
    const absl::flat_hash_map<std::string, MetricsPluginPtr>& name_to_custom_plugin_map,
    StepController& step_controller, Envoy::ProtobufWkt::Duration duration) {
  absl::StatusOr<nighthawk::client::CommandLineOptions> command_line_options_or =
      step_controller.GetCurrentCommandLineOptions();
  if (!command_line_options_or.ok()) {
    ENVOY_LOG_MISC(error, "Error constructing Nighthawk input: {}: {}",
                   command_line_options_or.status().code(),
                   command_line_options_or.status().message());
    return command_line_options_or.status();
  }
  nighthawk::client::CommandLineOptions command_line_options = command_line_options_or.value();
  // Overwrite the duration in the traffic template with the specified duration of the adjusting
  // or testing stage.
  *command_line_options.mutable_duration() = std::move(duration);

  ENVOY_LOG_MISC(info, "Sending load: {}", command_line_options.DebugString());
  Envoy::SystemTime start_time = time_source_.systemTime();
  absl::StatusOr<nighthawk::client::ExecutionResponse> nighthawk_response_or =
      nighthawk_service_client_.PerformNighthawkBenchmark(nighthawk_service_stub,
                                                          command_line_options);
  Envoy::SystemTime end_time = time_source_.systemTime();
  if (!nighthawk_response_or.ok()) {
    ENVOY_LOG_MISC(error, "Nighthawk Service error: {}: {}", nighthawk_response_or.status().code(),
                   nighthawk_response_or.status().message());
    return nighthawk_response_or.status();
  }
  nighthawk::client::ExecutionResponse nighthawk_response = nighthawk_response_or.value();
  LogGlobalResultExcludingStatistics(nighthawk_response);

  absl::StatusOr<BenchmarkResult> benchmark_result_or =
      metrics_evaluator_.AnalyzeNighthawkBenchmark(nighthawk_response, spec,
                                                   name_to_custom_plugin_map);
  if (!benchmark_result_or.ok()) {
    ENVOY_LOG_MISC(error, "Benchmark scoring error: {}: {}", benchmark_result_or.status().code(),
                   benchmark_result_or.status().message());
    return benchmark_result_or.status();
  }
  BenchmarkResult benchmark_result = benchmark_result_or.value();
  Envoy::TimestampUtil::systemClockToTimestamp(start_time, *benchmark_result.mutable_start_time());
  Envoy::TimestampUtil::systemClockToTimestamp(end_time, *benchmark_result.mutable_end_time());

  for (const MetricEvaluation& evaluation : benchmark_result.metric_evaluations()) {
    ENVOY_LOG_MISC(info, "Evaluation: {}", evaluation.DebugString());
  }
  step_controller.UpdateAndRecompute(benchmark_result);
  return benchmark_result;
}

absl::StatusOr<AdaptiveLoadSessionOutput> AdaptiveLoadControllerImpl::PerformAdaptiveLoadSession(
    nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
    const AdaptiveLoadSessionSpec& input_spec) {
  AdaptiveLoadSessionSpec spec = session_spec_proto_helper_.SetSessionSpecDefaults(input_spec);
  absl::Status validation_status = session_spec_proto_helper_.CheckSessionSpec(spec);
  if (!validation_status.ok()) {
    ENVOY_LOG_MISC(error, "Validation failed: {}", validation_status.message());
    return validation_status;
  }
  absl::flat_hash_map<std::string, MetricsPluginPtr> name_to_custom_metrics_plugin_map =
      LoadMetricsPlugins(spec);
  StepControllerPtr step_controller = LoadStepControllerPluginFromSpec(spec);
  AdaptiveLoadSessionOutput output;

  // Threshold specs are reproduced in the output proto for convenience.
  for (const nighthawk::adaptive_load::MetricSpecWithThreshold& threshold :
       spec.metric_thresholds()) {
    *output.mutable_metric_thresholds()->Add() = threshold;
  }

  // Perform adjusting stage:
  Envoy::MonotonicTime start_time = time_source_.monotonicTime();
  std::string doom_reason;
  do {
    absl::StatusOr<BenchmarkResult> result_or = PerformAndAnalyzeNighthawkBenchmark(
        nighthawk_service_stub, spec, name_to_custom_metrics_plugin_map, *step_controller,
        spec.measuring_period());
    if (!result_or.ok()) {
      return result_or.status();
    }
    BenchmarkResult result = result_or.value();
    *output.mutable_adjusting_stage_results()->Add() = result;

    if (spec.has_benchmark_cooldown_duration()) {
      ENVOY_LOG_MISC(info, "Cooling down before the next benchmark for duration: {}",
                     spec.benchmark_cooldown_duration());
      uint64_t sleep_time_ms = Envoy::Protobuf::util::TimeUtil::DurationToMilliseconds(
          spec.benchmark_cooldown_duration());
      absl::SleepFor(absl::Milliseconds(sleep_time_ms));
    }

    const std::chrono::nanoseconds time_limit_ns(
        Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(spec.convergence_deadline()));
    const auto elapsed_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        time_source_.monotonicTime() - start_time);
    if (elapsed_time_ns > time_limit_ns) {
      std::string message = absl::StrFormat("Failed to converge before deadline of %.2f seconds.",
                                            time_limit_ns.count() / 1e9);
      ENVOY_LOG_MISC(error, message);
      return absl::DeadlineExceededError(message);
    }
  } while (!step_controller->IsConverged() && !step_controller->IsDoomed(doom_reason));

  if (step_controller->IsDoomed(doom_reason)) {
    std::string message =
        absl::StrCat("Step controller determined that it can never converge: ", doom_reason);
    ENVOY_LOG_MISC(error, message);
    return absl::AbortedError(message);
  }

  // Perform testing stage:
  absl::StatusOr<BenchmarkResult> result_or = PerformAndAnalyzeNighthawkBenchmark(
      nighthawk_service_stub, spec, name_to_custom_metrics_plugin_map, *step_controller,
      spec.testing_stage_duration());
  if (!result_or.ok()) {
    return result_or.status();
  }
  *output.mutable_testing_stage_result() = result_or.value();
  return output;
}

} // namespace Nighthawk
