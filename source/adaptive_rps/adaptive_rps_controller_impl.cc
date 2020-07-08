#include <chrono>

#include "envoy/common/exception.h"

#include "nighthawk/adaptive_rps/adaptive_rps_controller.h"
#include "nighthawk/adaptive_rps/metrics_plugin.h"
#include "nighthawk/adaptive_rps/scoring_function.h"
#include "nighthawk/adaptive_rps/step_controller.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_rps/adaptive_rps.pb.h"
#include "api/adaptive_rps/metric_spec.pb.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"
#include "api/client/service.grpc.pb.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "adaptive_rps/metrics_plugin_impl.h"
#include "adaptive_rps/plugin_util.h"

namespace Nighthawk {
namespace AdaptiveRps {

namespace {

using nighthawk::adaptive_rps::AdaptiveRpsSessionOutput;
using nighthawk::adaptive_rps::AdaptiveRpsSessionSpec;
using nighthawk::adaptive_rps::BenchmarkResult;
using nighthawk::adaptive_rps::MetricEvaluation;
using nighthawk::adaptive_rps::MetricSpec;
using nighthawk::adaptive_rps::MetricSpecWithThreshold;
using nighthawk::adaptive_rps::MetricsPluginConfig;
using nighthawk::adaptive_rps::OUTSIDE_THRESHOLD;
using nighthawk::adaptive_rps::ThresholdSpec;
using nighthawk::adaptive_rps::WITHIN_THRESHOLD;

// Runs a single benchmark using a Nighthawk Service. Unconditionally returns a
// nighthawk::client::ExecutionResponse. The ExecutionResponse may contain an error reported by the
// Nighthawk Service. If we encounter a gRPC error communicating with the Nighthawk Service, we
// insert the error code and message into the ExecutionResponse.
nighthawk::client::ExecutionResponse
PerformNighthawkBenchmark(nighthawk::client::NighthawkService::Stub* nighthawk_service_stub,
                          const AdaptiveRpsSessionSpec& spec, int rps,
                          Envoy::Protobuf::Duration duration) noexcept {
  nighthawk::client::CommandLineOptions options = spec.nighthawk_traffic_template();
  *options.mutable_duration() = duration;
  options.mutable_requests_per_second()->set_value(rps);
  options.mutable_open_loop()->set_value(false);

  nighthawk::client::ExecutionRequest request;
  *request.mutable_start_request()->mutable_options() = options;

  ::grpc::ClientContext context;
  std::shared_ptr<::grpc::ClientReaderWriter<nighthawk::client::ExecutionRequest,
                                             nighthawk::client::ExecutionResponse>>
      stream(nighthawk_service_stub->ExecutionStream(&context));

  stream->Write(request);
  stream->WritesDone();

  nighthawk::client::ExecutionResponse response;
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

// Analyzes a single Nighthawk Service benchmark result against configured MetricThresholds.
// Queries outside MetricsPlugins if configured and/or uses "builtin" plugin to check Nighthawk
// Service stats and counters.
BenchmarkResult
AnalyzeNighthawkBenchmark(const nighthawk::client::ExecutionResponse& nighthawk_response,
                          const AdaptiveRpsSessionSpec& spec) noexcept {
  BenchmarkResult benchmark_result;

  *benchmark_result.mutable_nighthawk_service_output() = nighthawk_response.output();

  if (nighthawk_response.error_detail().code() != ::grpc::OK) {
    return benchmark_result;
  }

  absl::flat_hash_map<std::string, MetricsPluginPtr> name_to_plugin;
  name_to_plugin["builtin"] =
      std::make_unique<NighthawkStatsEmulatedMetricsPlugin>(nighthawk_response.output());
  for (const MetricsPluginConfig& config : spec.metrics_plugin_configs()) {
    name_to_plugin[config.name()] = LoadMetricsPlugin(config);
  }

  for (const MetricSpecWithThreshold& metric_threshold : spec.metric_thresholds()) {
    MetricEvaluation evaluation;
    *evaluation.mutable_metric_spec() = metric_threshold.metric_spec();
    *evaluation.mutable_threshold_spec() = metric_threshold.threshold_spec();

    double metric_value =
        name_to_plugin[metric_threshold.metric_spec().metrics_plugin_name()]->GetMetricByName(
            metric_threshold.metric_spec().metric_name());
    evaluation.set_metric_value(metric_value);
    if (metric_threshold.threshold_spec().has_lower_threshold()) {
      evaluation.mutable_threshold_check_result()->set_simple_threshold_status(
          metric_value >= metric_threshold.threshold_spec().lower_threshold().value()
              ? WITHIN_THRESHOLD
              : OUTSIDE_THRESHOLD);
    } else if (metric_threshold.threshold_spec().has_upper_threshold()) {
      evaluation.mutable_threshold_check_result()->set_simple_threshold_status(
          metric_value <= metric_threshold.threshold_spec().upper_threshold().value()
              ? WITHIN_THRESHOLD
              : OUTSIDE_THRESHOLD);
    } else if (metric_threshold.threshold_spec().has_scoring_function()) {
      ScoringFunctionPtr scoring_function =
          LoadScoringFunctionPlugin(metric_threshold.threshold_spec().scoring_function());
      evaluation.mutable_threshold_check_result()->set_threshold_score(
          scoring_function->EvaluateMetric(metric_value));
    }
    *benchmark_result.mutable_metric_evaluations()->Add() = evaluation;
  }
  for (const MetricSpec& metric_spec : spec.informational_metric_specs()) {
    MetricEvaluation evaluation;
    *evaluation.mutable_metric_spec() = metric_spec;

    double metric_value = name_to_plugin[metric_spec.metrics_plugin_name()]->GetMetricByName(
        metric_spec.metric_name());
    evaluation.set_metric_value(metric_value);
    *benchmark_result.mutable_metric_evaluations()->Add() = evaluation;
  }
  return benchmark_result;
}

// Performs a benchmark via a Nighthawk Service, then hands the result off for analysis.
BenchmarkResult PerformAndAnalyzeNighthawkBenchmark(
    nighthawk::client::NighthawkService::Stub* nighthawk_service_stub,
    const AdaptiveRpsSessionSpec& spec, int rps, Envoy::Protobuf::Duration duration) {
  nighthawk::client::ExecutionResponse response =
      PerformNighthawkBenchmark(nighthawk_service_stub, spec, rps, duration);
  return AnalyzeNighthawkBenchmark(response, spec);
}

// Checks whether a session spec is valid: No forbidden fields in Nighthawk traffic spec; no
// references to missing plugins (step controller, metric, scoring function); no nonexistent
// metric names; all weights set or no weights set.
absl::Status
CheckSessionSpec(const nighthawk::adaptive_rps::AdaptiveRpsSessionSpec& spec) noexcept {
  std::string errors;

  if (spec.nighthawk_traffic_template().has_duration()) {
    errors += "nighthawk_traffic_template should not have |duration| set. Set |measuring_period| "
              "and |testing_stage_duration| in the AdaptiveRpsSessionSpec proto instead.\n";
  }
  if (spec.nighthawk_traffic_template().has_requests_per_second()) {
    errors += "nighthawk_traffic_template should not have |requests_per_second| set. RPS will be "
              "set dynamically according to limits in |step_controller_config|.\n";
  }
  if (spec.nighthawk_traffic_template().has_open_loop()) {
    errors += "nighthawk_traffic_template should not have |open_loop| set. Adaptive RPS always "
              "operates in open loop mode.\n";
  }

  absl::flat_hash_map<std::string, MetricsPluginPtr> plugin_from_name;
  std::vector<std::string> plugin_names = {"builtin"};
  plugin_from_name["builtin"] =
      std::make_unique<NighthawkStatsEmulatedMetricsPlugin>(nighthawk::client::Output());
  for (const MetricsPluginConfig& config : spec.metrics_plugin_configs()) {
    try {
      plugin_from_name[config.name()] = LoadMetricsPlugin(config);
    } catch (Envoy::EnvoyException exception) {
      errors += absl::StrCat("MetricsPlugin not found: ", exception.what(), "\n");
    }
    plugin_names.push_back(config.name());
  }

  try {
    LoadStepControllerPlugin(spec.step_controller_config());
  } catch (Envoy::EnvoyException exception) {
    errors += absl::StrCat("StepController plugin not found: ", exception.what(), "\n");
  }

  std::vector<MetricSpec> all_metric_specs;

  int count_with_weight = 0;
  int count_without_weight = 0;
  for (const MetricSpecWithThreshold& metric_threshold : spec.metric_thresholds()) {
    all_metric_specs.push_back(metric_threshold.metric_spec());

    if (metric_threshold.threshold_spec().has_weight()) {
      ++count_with_weight;
    } else {
      ++count_without_weight;
    }

    if (metric_threshold.threshold_spec().has_scoring_function()) {
      try {
        LoadScoringFunctionPlugin(metric_threshold.threshold_spec().scoring_function());
      } catch (Envoy::EnvoyException exception) {
        errors += absl::StrCat("ScoringFunction plugin not found: ", exception.what(), "\n");
      }
    }
  }
  if (count_with_weight > 0 && count_without_weight > 0) {
    errors += "Either all metric thresholds or none must have weights set.\n";
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
        errors += "Metric named '" + metric_spec.metric_name() + "' not implemented by plugin '" +
                  metric_spec.metrics_plugin_name() +
                  "'. Metrics implemented: " + absl::StrJoin(supported_metrics, ", ") + ".\n";
      }
    } else {
      errors += "MetricSpec referred to nonexistent metrics_plugin_name '" +
                metric_spec.metrics_plugin_name() +
                "'. You must declare the plugin in metrics_plugin_configs or use plugin 'builtin'. "
                "Available plugins: " +
                absl::StrJoin(plugin_names, ", ") + ".\n";
    }
  }

  if (errors.length() > 0) {
    return absl::InvalidArgumentError(errors);
  }
  return absl::OkStatus();
}

} // namespace

AdaptiveRpsSessionOutput
PerformAdaptiveRpsSession(nighthawk::client::NighthawkService::Stub* nighthawk_service_stub,
                          const AdaptiveRpsSessionSpec& spec, std::ostream* diagnostic_ostream,
                          Envoy::TimeSource* time_source) noexcept {
  AdaptiveRpsSessionOutput output;

  absl::Status validation_status = CheckSessionSpec(spec);
  if (!validation_status.ok()) {
    output.mutable_session_status()->set_code(static_cast<int>(validation_status.code()));
    output.mutable_session_status()->set_message(std::string(validation_status.message()));
    return output;
  }

  StepControllerPtr step_controller = LoadStepControllerPlugin(spec.step_controller_config());

  Envoy::MonotonicTime start_time = time_source->monotonicTime();
  while (!step_controller->IsConverged()) {
    if (std::chrono::duration_cast<std::chrono::seconds>(time_source->monotonicTime() - start_time)
            .count() > spec.convergence_deadline().seconds()) {
      output.mutable_session_status()->set_code(grpc::DEADLINE_EXCEEDED);
      output.mutable_session_status()->set_message(
          absl::StrCat("Failed to converge before deadline of ",
                       spec.convergence_deadline().seconds(), " seconds."));
      return output;
    }

    if (diagnostic_ostream != nullptr) {
      *diagnostic_ostream << "Trying " << step_controller->GetCurrentRps() << " rps...\n";
    }

    BenchmarkResult result = PerformAndAnalyzeNighthawkBenchmark(
        nighthawk_service_stub, spec, step_controller->GetCurrentRps(), spec.measuring_period());

    if (diagnostic_ostream != nullptr) {
      for (const MetricEvaluation& evaluation : result.metric_evaluations()) {
        *diagnostic_ostream << evaluation.DebugString() << "\n";
      }
    }

    *output.mutable_adjusting_stage_results()->Add() = result;
    step_controller->UpdateAndRecompute(result);
  }

  if (diagnostic_ostream != nullptr) {
    *diagnostic_ostream << "Testing stage: " << step_controller->GetCurrentRps() << " rps...\n";
  }

  *output.mutable_testing_stage_result() = PerformAndAnalyzeNighthawkBenchmark(
      nighthawk_service_stub, spec, step_controller->GetCurrentRps(),
      spec.testing_stage_duration());

  if (diagnostic_ostream != nullptr) {
    for (const MetricEvaluation& evaluation : output.testing_stage_result().metric_evaluations()) {
      *diagnostic_ostream << evaluation.DebugString() << "\n";
    }
  }
  return output;
}

} // namespace AdaptiveRps
} // namespace Nighthawk
