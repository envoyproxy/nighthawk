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

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/metric_spec.pb.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"
#include "api/client/service.grpc.pb.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "adaptive_load/metrics_plugin_impl.h"
#include "adaptive_load/plugin_loader.h"

namespace Nighthawk {

namespace {

using namespace Envoy; // for Envoy::Logger

using nighthawk::adaptive_load::AdaptiveLoadSessionOutput;
using nighthawk::adaptive_load::AdaptiveLoadSessionSpec;
using nighthawk::adaptive_load::BenchmarkResult;
using nighthawk::adaptive_load::MetricEvaluation;
using nighthawk::adaptive_load::MetricSpec;
using nighthawk::adaptive_load::MetricSpecWithThreshold;
using nighthawk::adaptive_load::ThresholdSpec;

/**
 * Runs a single benchmark using a Nighthawk Service.
 *
 * @param nighthawk_service_stub Nighthawk Service gRPC stub.
 * @param command_line_options Full input for the Nighthawk Service, except for the duration, as
 * produced by the StepController.
 * @param duration Duration to insert into the final benchmark request.
 *
 * @return ExecutionResponse If we reached the Nighthawk Service, the raw ExecutionResponse proto,
 * possibly containing an error message from Nighthawk Service; if we could not reach Nighthawk
 * Service, a synthetic ExecutionRequest proto with |status| indicating a communication failure.
 */
nighthawk::client::ExecutionResponse PerformNighthawkBenchmark(
    nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
    const nighthawk::client::CommandLineOptions& command_line_options,
    Envoy::Protobuf::Duration duration) {
  nighthawk::client::CommandLineOptions options = command_line_options;
  *options.mutable_duration() = duration;
  options.mutable_open_loop()->set_value(false);

  nighthawk::client::ExecutionRequest request;
  nighthawk::client::ExecutionResponse response;
  *request.mutable_start_request()->mutable_options() = options;

  ::grpc::ClientContext context;
  std::shared_ptr<::grpc::ClientReaderWriterInterface<nighthawk::client::ExecutionRequest,
                                                      nighthawk::client::ExecutionResponse>>
      stream(nighthawk_service_stub->ExecutionStream(&context));

  stream->Write(request);
  stream->WritesDone();
  if (!stream->Read(&response)) {
    response.mutable_error_detail()->set_code(::grpc::UNKNOWN);
    response.mutable_error_detail()->set_message("Nighthawk Service did not send a response.");
  }
  ::grpc::Status status = stream->Finish();
  if (!status.ok()) {
    response.mutable_error_detail()->set_code(status.error_code());
    response.mutable_error_detail()->set_message(status.error_message());
  }
  return response;
}

/**
 * Analyzes a single Nighthawk Service benchmark result against configured MetricThresholds.
 * Queries outside MetricsPlugins if configured and/or uses "nighthawk.builtin" plugin to check
 * Nighthawk Service stats and counters.
 *
 * @param nighthawk_response Proto returned from Nighthawk Service describing a single benchmark
 * session.
 * @param spec Top-level proto defining the adaptive load session.
 * @param name_to_custom_metrics_plugin_map Common map from plugin names to MetricsPlugins, loaded
 * and initialized once at the beginning of the session and passed to all calls of this function.
 *
 * @return BenchmarkResult Proto containing metric scores for this Nighthawk Service benchmark
 * session, or an error propagated from the Nighthawk Service or MetricsPlugins.
 */
BenchmarkResult AnalyzeNighthawkBenchmark(
    const nighthawk::client::ExecutionResponse& nighthawk_response,
    const AdaptiveLoadSessionSpec& spec,
    const absl::flat_hash_map<std::string, MetricsPluginPtr>& name_to_custom_metrics_plugin_map) {
  BenchmarkResult benchmark_result;

  *benchmark_result.mutable_nighthawk_service_output() = nighthawk_response.output();

  absl::flat_hash_map<std::string, MetricsPlugin*> name_to_plugin_map;
  for (const auto& pair : name_to_custom_metrics_plugin_map) {
    name_to_plugin_map[pair.first] = pair.second.get();
  }
  auto builtin_plugin =
      std::make_unique<NighthawkStatsEmulatedMetricsPlugin>(nighthawk_response.output());
  name_to_plugin_map["nighthawk.builtin"] = builtin_plugin.get();

  *benchmark_result.mutable_status() = nighthawk_response.error_detail();
  if (nighthawk_response.error_detail().code() != ::grpc::OK) {
    return benchmark_result;
  }
  // MetricSpecs in original order of definition.
  std::vector<const nighthawk::adaptive_load::MetricSpec*> metric_specs;
  // Pointer to the corresponding ThresholdSpec, or nullptr for informational metrics.
  absl::flat_hash_map<const nighthawk::adaptive_load::MetricSpec*,
                      const nighthawk::adaptive_load::ThresholdSpec*>
      threshold_spec_from_metric_spec;
  for (const MetricSpecWithThreshold& metric_threshold : spec.metric_thresholds()) {
    metric_specs.push_back(&metric_threshold.metric_spec());
    threshold_spec_from_metric_spec[&metric_threshold.metric_spec()] =
        &metric_threshold.threshold_spec();
  }
  for (const MetricSpec& metric_spec : spec.informational_metric_specs()) {
    metric_specs.push_back(&metric_spec);
    threshold_spec_from_metric_spec[&metric_spec] = nullptr;
  }
  std::vector<std::string> errors;
  for (const nighthawk::adaptive_load::MetricSpec* metric_spec : metric_specs) {
    MetricEvaluation evaluation;
    evaluation.set_metric_id(
        absl::StrCat(metric_spec->metrics_plugin_name(), "/", metric_spec->metric_name()));
    const absl::StatusOr<double> metric_value_or =
        name_to_plugin_map[metric_spec->metrics_plugin_name()]->GetMetricByName(
            metric_spec->metric_name());
    if (metric_value_or.ok()) {
      const double metric_value = metric_value_or.value();
      evaluation.set_metric_value(metric_value);
      const nighthawk::adaptive_load::ThresholdSpec* threshold_spec =
          threshold_spec_from_metric_spec[metric_spec];
      if (threshold_spec == nullptr) {
        // Informational metric.
        evaluation.set_weight(0.0);
      } else {
        evaluation.set_weight(threshold_spec->weight().value());
        absl::StatusOr<ScoringFunctionPtr> scoring_function_or =
            LoadScoringFunctionPlugin(threshold_spec->scoring_function());
        RELEASE_ASSERT(scoring_function_or.ok(),
                       absl::StrCat("ScoringFunction plugin loading error should have been caught "
                                    "during input validation: ",
                                    scoring_function_or.status().message()));
        ScoringFunctionPtr scoring_function = std::move(scoring_function_or.value());
        evaluation.set_threshold_score(scoring_function->EvaluateMetric(metric_value));
      }
    } else {
      errors.emplace_back(absl::StrCat("Error calling MetricsPlugin '",
                                       metric_spec->metrics_plugin_name(), ": ",
                                       metric_value_or.status().message()));
    }
    *benchmark_result.mutable_metric_evaluations()->Add() = evaluation;
  }
  if (errors.empty()) {
    benchmark_result.mutable_status()->set_code(::grpc::OK);
  } else {
    benchmark_result.mutable_status()->set_code(::grpc::INTERNAL);
    benchmark_result.mutable_status()->set_message(absl::StrJoin(errors, "\n"));
  }
  return benchmark_result;
}

/**
 * Performs a benchmark via a Nighthawk Service, then hands the result off for analysis.
 *
 * @param nighthawk_service_stub Nighthawk Service gRPC stub.
 * @param spec Proto describing the overall adaptive load session.
 * @param name_to_custom_plugin_map Common map from plugin names to MetricsPlugins loaded and
 * initialized once at the beginning of the session and passed to all calls of this function.
 * @param command_line_options Full input for the Nighthawk Service, except for the duration.
 * @param duration Duration to insert into the benchmark request.
 *
 * @return BenchmarkResult Proto containing raw Nighthawk Service results, metric values, and metric
 * scores.
 */
BenchmarkResult PerformAndAnalyzeNighthawkBenchmark(
    nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
    const AdaptiveLoadSessionSpec& spec,
    const absl::flat_hash_map<std::string, MetricsPluginPtr>& name_to_custom_plugin_map,
    const nighthawk::client::CommandLineOptions& command_line_options,
    Envoy::Protobuf::Duration duration) {
  nighthawk::client::ExecutionResponse response =
      PerformNighthawkBenchmark(nighthawk_service_stub, command_line_options, duration);
  return AnalyzeNighthawkBenchmark(response, spec, name_to_custom_plugin_map);
}

/**
 * Returns a copy of the input spec with default values inserted.
 *
 * @param original_spec Valid adaptive load session spec.
 *
 * @return Adaptive load session spec with default values inserted.
 */
AdaptiveLoadSessionSpec SetDefaults(const AdaptiveLoadSessionSpec& original_spec) {
  AdaptiveLoadSessionSpec spec = original_spec;
  if (!spec.has_measuring_period()) {
    spec.mutable_measuring_period()->set_seconds(10);
  }
  if (!spec.has_convergence_deadline()) {
    spec.mutable_convergence_deadline()->set_seconds(300);
  }
  if (!spec.has_testing_stage_duration()) {
    spec.mutable_testing_stage_duration()->set_seconds(30);
  }
  for (nighthawk::adaptive_load::MetricSpecWithThreshold& threshold :
       *spec.mutable_metric_thresholds()) {
    if (threshold.metric_spec().metrics_plugin_name().empty()) {
      threshold.mutable_metric_spec()->set_metrics_plugin_name("nighthawk.builtin");
    }
    if (!threshold.threshold_spec().has_weight()) {
      threshold.mutable_threshold_spec()->mutable_weight()->set_value(1.0);
    }
  }
  for (nighthawk::adaptive_load::MetricSpec& metric_spec :
       *spec.mutable_informational_metric_specs()) {
    if (metric_spec.metrics_plugin_name().empty()) {
      metric_spec.set_metrics_plugin_name("nighthawk.builtin");
    }
  }
  return spec;
}

/**
 * Checks whether a session spec is valid: No forbidden fields in Nighthawk traffic spec; no bad
 * plugin references or bad plugin configurations (step controller, metric, scoring function); no
 * nonexistent metric names. Reports all errors in one pass.
 *
 * @param spec A potentially invalid adaptive load session spec.
 *
 * @return Status OK if no problems were found, or InvalidArgument with all errors.
 */
absl::Status CheckSessionSpec(const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec) {
  std::vector<std::string> errors;
  if (spec.nighthawk_traffic_template().has_duration()) {
    errors.emplace_back(
        "nighthawk_traffic_template should not have |duration| set. Set |measuring_period| "
        "and |testing_stage_duration| in the AdaptiveLoadSessionSpec proto instead.");
  }
  if (spec.nighthawk_traffic_template().has_open_loop()) {
    errors.emplace_back(
        "nighthawk_traffic_template should not have |open_loop| set. Adaptive Load always "
        "operates in open loop mode.");
  }
  absl::flat_hash_map<std::string, MetricsPluginPtr> plugin_from_name;
  std::vector<std::string> plugin_names = {"nighthawk.builtin"};
  plugin_from_name["nighthawk.builtin"] =
      std::make_unique<NighthawkStatsEmulatedMetricsPlugin>(nighthawk::client::Output());
  for (const envoy::config::core::v3::TypedExtensionConfig& config :
       spec.metrics_plugin_configs()) {
    plugin_names.push_back(config.name());
    absl::StatusOr<MetricsPluginPtr> metrics_plugin_or = LoadMetricsPlugin(config);
    if (!metrics_plugin_or.ok()) {
      errors.emplace_back(
          absl::StrCat("Failed to load MetricsPlugin: ", metrics_plugin_or.status().message()));
      continue;
    }
    plugin_from_name[config.name()] = std::move(metrics_plugin_or.value());
  }
  absl::StatusOr<StepControllerPtr> step_controller_or =
      LoadStepControllerPlugin(spec.step_controller_config(), spec.nighthawk_traffic_template());
  if (!step_controller_or.ok()) {
    errors.emplace_back(absl::StrCat("Failed to load StepController plugin: ",
                                     step_controller_or.status().message()));
  }
  std::vector<MetricSpec> all_metric_specs;
  for (const MetricSpecWithThreshold& metric_threshold : spec.metric_thresholds()) {
    all_metric_specs.push_back(metric_threshold.metric_spec());
    absl::StatusOr<ScoringFunctionPtr> scoring_function_or =
        LoadScoringFunctionPlugin(metric_threshold.threshold_spec().scoring_function());
    if (!scoring_function_or.ok()) {
      errors.emplace_back(absl::StrCat("Failed to load ScoringFunction plugin: ",
                                       scoring_function_or.status().message()));
    }
  }
  for (const MetricSpec& metric_spec : spec.informational_metric_specs()) {
    all_metric_specs.push_back(metric_spec);
  }
  for (const MetricSpec& metric_spec : all_metric_specs) {
    if (plugin_from_name.contains(metric_spec.metrics_plugin_name())) {
      std::vector<std::string> supported_metrics =
          plugin_from_name[metric_spec.metrics_plugin_name()]->GetAllSupportedMetricNames();
      if (std::find(supported_metrics.begin(), supported_metrics.end(),
                    metric_spec.metric_name()) == supported_metrics.end()) {
        errors.emplace_back(
            absl::StrCat("Metric named '", metric_spec.metric_name(),
                         "' not implemented by plugin '", metric_spec.metrics_plugin_name(),
                         "'. Metrics implemented: ", absl::StrJoin(supported_metrics, ", "), "."));
      }
    } else {
      errors.emplace_back(absl::StrCat(
          "MetricSpec referred to nonexistent metrics_plugin_name '",
          metric_spec.metrics_plugin_name(),
          "'. You must declare the plugin in metrics_plugin_configs or use plugin ",
          "'nighthawk.builtin'. Available plugins: ", absl::StrJoin(plugin_names, ", "), "."));
    }
  }
  if (errors.size() > 0) {
    return absl::InvalidArgumentError(absl::StrJoin(errors, "\n"));
  }
  return absl::OkStatus();
}

} // namespace

AdaptiveLoadSessionOutput PerformAdaptiveLoadSession(
    nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
    const AdaptiveLoadSessionSpec& input_spec, Envoy::TimeSource& time_source) {
  AdaptiveLoadSessionOutput output;

  AdaptiveLoadSessionSpec spec = SetDefaults(input_spec);
  absl::Status validation_status = CheckSessionSpec(spec);
  if (!validation_status.ok()) {
    output.mutable_session_status()->set_code(static_cast<int>(validation_status.code()));
    output.mutable_session_status()->set_message(std::string(validation_status.message()));
    return output;
  }

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

  absl::StatusOr<StepControllerPtr> step_controller_or =
      LoadStepControllerPlugin(spec.step_controller_config(), spec.nighthawk_traffic_template());
  RELEASE_ASSERT(
      step_controller_or.ok(),
      absl::StrCat(
          "StepController plugin loading error should have been caught during input validation: ",
          step_controller_or.status().message()));
  StepControllerPtr step_controller = std::move(step_controller_or.value());

  for (const nighthawk::adaptive_load::MetricSpecWithThreshold& threshold :
       spec.metric_thresholds()) {
    *output.mutable_metric_thresholds()->Add() = threshold;
  }

  Envoy::MonotonicTime start_time = time_source.monotonicTime();
  while (!step_controller->IsConverged()) {
    std::string doom_reason;
    if (step_controller->IsDoomed(doom_reason)) {
      output.mutable_session_status()->set_code(grpc::ABORTED);
      std::string message = "Step controller determined that it can never converge: " + doom_reason;
      output.mutable_session_status()->set_message(message);
      ENVOY_LOG_MISC(info, message);
      return output;
    }
    if (std::chrono::duration_cast<std::chrono::seconds>(time_source.monotonicTime() - start_time)
            .count() > spec.convergence_deadline().seconds()) {
      output.mutable_session_status()->set_code(grpc::DEADLINE_EXCEEDED);
      std::string message = absl::StrCat("Failed to converge before deadline of ",
                                         spec.convergence_deadline().seconds(), " seconds.");
      output.mutable_session_status()->set_message(message);
      ENVOY_LOG_MISC(info, message);
      return output;
    }
    
    absl::StatusOr<nighthawk::client::CommandLineOptions> command_line_options_or =
        step_controller->GetCurrentCommandLineOptions();
    if (!command_line_options_or.ok()) {
      std::string message = absl::StrCat("Error setting Nighthawk input: ",
                                         command_line_options_or.status().message());
      output.mutable_session_status()->set_code(grpc::ABORTED);
      output.mutable_session_status()->set_message(message);
      ENVOY_LOG_MISC(info, message);
      return output;
    }
    nighthawk::client::CommandLineOptions command_line_options = command_line_options_or.value();
    ENVOY_LOG_MISC(info, "Adjusting Stage: Trying load: {}", command_line_options.DebugString());
    BenchmarkResult result = PerformAndAnalyzeNighthawkBenchmark(
        nighthawk_service_stub, spec, name_to_custom_metrics_plugin_map, command_line_options,
        spec.measuring_period());
    for (const MetricEvaluation& evaluation : result.metric_evaluations()) {
      ENVOY_LOG_MISC(info, "Evaluation: {}", evaluation.DebugString());
    }
    *output.mutable_adjusting_stage_results()->Add() = result;
    step_controller->UpdateAndRecompute(result);
  }

  absl::StatusOr<nighthawk::client::CommandLineOptions> command_line_options_or =
      step_controller->GetCurrentCommandLineOptions();
  if (!command_line_options_or.ok()) {
    std::string message =
        absl::StrCat("Error setting Nighthawk input: ", command_line_options_or.status().message());
    output.mutable_session_status()->set_code(grpc::ABORTED);
    output.mutable_session_status()->set_message(message);
    ENVOY_LOG_MISC(info, message);
    return output;
  }
  nighthawk::client::CommandLineOptions command_line_options = command_line_options_or.value();
  ENVOY_LOG_MISC(info, "Testing Stage with load: {}", command_line_options.DebugString());
  *output.mutable_testing_stage_result() = PerformAndAnalyzeNighthawkBenchmark(
      nighthawk_service_stub, spec, name_to_custom_metrics_plugin_map, command_line_options,
      spec.testing_stage_duration());
  for (const MetricEvaluation& evaluation : output.testing_stage_result().metric_evaluations()) {
    ENVOY_LOG_MISC(info, "Evaluation: {}", evaluation.DebugString());
  }
  return output;
}

} // namespace Nighthawk
