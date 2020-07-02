#include "adaptive_rps/adaptive_rps_controller.h"

#include "nighthawk/adaptive_rps/custom_metric_evaluator.h"
#include "nighthawk/adaptive_rps/metrics_plugin.h"
#include "nighthawk/adaptive_rps/step_controller.h"

#include "adaptive_rps/metrics_plugin_impl.h"
#include "adaptive_rps/plugin_util.h"

#include "api/adaptive_rps/adaptive_rps.pb.h"
#include "api/adaptive_rps/metric_spec.pb.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"
#include "api/client/service.grpc.pb.h"

#include "common/protobuf/protobuf.h"
#include "google/protobuf/duration.pb.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"

namespace Nighthawk {
namespace AdaptiveRps {

namespace {

using nighthawk::adaptive_rps::AdaptiveRpsSessionOutput;
using nighthawk::adaptive_rps::AdaptiveRpsSessionSpec;
using nighthawk::adaptive_rps::BenchmarkResult;
using nighthawk::adaptive_rps::WITHIN_THRESHOLD;
using nighthawk::adaptive_rps::MetricEvaluation;
using nighthawk::adaptive_rps::MetricSpec;
using nighthawk::adaptive_rps::MetricsPluginConfig;
using nighthawk::adaptive_rps::OUTSIDE_THRESHOLD;

nighthawk::client::ExecutionResponse
PerformNighthawkBenchmark(nighthawk::client::NighthawkService::Stub* nighthawk_service_stub,
                          const AdaptiveRpsSessionSpec& spec, int rps,
                          google::protobuf::Duration duration) {
  nighthawk::client::CommandLineOptions options = spec.nighthawk_traffic_template();
  *options.mutable_duration() = duration;
  options.mutable_requests_per_second()->set_value(rps);
  options.mutable_open_loop()->set_value(false);

  nighthawk::client::ExecutionRequest request;
  *request.mutable_start_request()->mutable_options() = options;

  grpc::ClientContext context;
  std::shared_ptr<grpc::ClientReaderWriter<nighthawk::client::ExecutionRequest,
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

BenchmarkResult
AnalyzeNighthawkBenchmark(const nighthawk::client::ExecutionResponse& nighthawk_response,
                          const AdaptiveRpsSessionSpec& spec) {
  BenchmarkResult benchmark_result;

  *benchmark_result.mutable_nighthawk_service_output() = nighthawk_response.output();

  if (nighthawk_response.error_detail().code() > 0) {
    return benchmark_result;
  }

  absl::flat_hash_map<std::string, MetricsPluginPtr> name_to_plugin;
  name_to_plugin["builtin"] = std::make_unique<InternalMetricsPlugin>(nighthawk_response.output());
  for (const MetricsPluginConfig& config : spec.metrics_plugin_configs()) {
    name_to_plugin[config.name()] = LoadMetricsPlugin(config);
  }

  for (const MetricSpec& metric_spec : spec.metric_specs()) {
    MetricEvaluation evaluation;
    *evaluation.mutable_metric_spec() = metric_spec;

    double metric_value = name_to_plugin[metric_spec.metrics_plugin_name()]->GetMetricByName(
        metric_spec.metric_name());
    evaluation.set_metric_value(metric_value);
    if (metric_spec.lower_threshold() > 0) {
      evaluation.set_threshold_status(
          metric_value >= metric_spec.lower_threshold() ? WITHIN_THRESHOLD : OUTSIDE_THRESHOLD);
    } else if (metric_spec.upper_threshold() > 0) {
      evaluation.set_threshold_status(
          metric_value <= metric_spec.upper_threshold() ? WITHIN_THRESHOLD : OUTSIDE_THRESHOLD);
    } else {
      CustomMetricEvaluatorPtr metric_evaluator =
          LoadCustomMetricEvaluatorPlugin(metric_spec.custom_metric_evaluator());
      evaluation.set_threshold_score(metric_evaluator->EvaluateMetric(metric_value));
    }
    *benchmark_result.mutable_metric_evaluations()->Add() = evaluation;
  }
  return benchmark_result;
}

BenchmarkResult PerformAndAnalyzeNighthawkBenchmark(
    nighthawk::client::NighthawkService::Stub* nighthawk_service_stub,
    const AdaptiveRpsSessionSpec& spec, int rps, google::protobuf::Duration duration) {
  nighthawk::client::ExecutionResponse response =
      PerformNighthawkBenchmark(nighthawk_service_stub, spec, rps, duration);
  return AnalyzeNighthawkBenchmark(response, spec);
}

} // namespace

AdaptiveRpsSessionOutput
PerformAdaptiveRpsSession(nighthawk::client::NighthawkService::Stub* nighthawk_service_stub,
                          const AdaptiveRpsSessionSpec& spec) {
  AdaptiveRpsSessionOutput output;

  StepControllerPtr step_controller = LoadStepControllerPlugin(spec.step_controller_config());

  while (!step_controller->IsConverged()) {
    BenchmarkResult result = PerformAndAnalyzeNighthawkBenchmark(
        nighthawk_service_stub, spec, step_controller->GetCurrentRps(), spec.measuring_period());
    *output.mutable_adjusting_stage_results()->Add() = result;
    step_controller->UpdateAndRecompute(result);
  }
  *output.mutable_testing_stage_result() = PerformAndAnalyzeNighthawkBenchmark(
      nighthawk_service_stub, spec, step_controller->GetCurrentRps(),
      spec.testing_stage_duration());
  return output;
}

} // namespace AdaptiveRps
} // namespace Nighthawk
