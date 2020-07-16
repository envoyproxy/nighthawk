#include "adaptive_load/step_controller_impl.h"

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/metric_spec.pb.h"
#include "api/adaptive_load/step_controller_impl.pb.h"

#include "adaptive_load/input_variable_setter_impl.h"
#include "adaptive_load/plugin_util.h"

namespace Nighthawk {
namespace AdaptiveLoad {

namespace {

using nighthawk::adaptive_load::BenchmarkResult;
using nighthawk::adaptive_load::ExponentialSearchStepControllerConfig;
using nighthawk::adaptive_load::MetricEvaluation;

// Adds all collected metric results according to their weights.
double TotalWeightedScore(const BenchmarkResult& benchmark_result) {
  double score = 0.0;
  double total_weight = 0.0;
  for (const MetricEvaluation& evaluation : benchmark_result.metric_evaluations()) {
    if (!evaluation.has_threshold_spec()) {
      // Metric was recorded for display purposes only.
      continue;
    }
    double weight = evaluation.threshold_spec().has_weight()
                        ? evaluation.threshold_spec().weight().value()
                        : 1.0;
    score += weight * evaluation.threshold_score();
    total_weight += weight;
  }
  return score / total_weight;
}

} // namespace

Envoy::ProtobufTypes::MessagePtr
ExponentialSearchStepControllerConfigFactory::createEmptyConfigProto() {
  return std::make_unique<ExponentialSearchStepControllerConfig>();
}

std::string ExponentialSearchStepControllerConfigFactory::name() const {
  return "exponential-search";
}

StepControllerPtr ExponentialSearchStepControllerConfigFactory::createStepController(
    const Envoy::Protobuf::Message& message,
    const nighthawk::client::CommandLineOptions& command_line_options_template) {
  const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  ExponentialSearchStepControllerConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<ExponentialSearchStepController>(config, command_line_options_template);
}

REGISTER_FACTORY(ExponentialSearchStepControllerConfigFactory, StepControllerConfigFactory);

ExponentialSearchStepController::ExponentialSearchStepController(
    const ExponentialSearchStepControllerConfig& config,
    const nighthawk::client::CommandLineOptions& command_line_options_template)
    : config_{config}, command_line_options_template_{command_line_options_template},
      input_variable_setter_{
          config.has_input_variable_setter()
              ? LoadInputVariableSetterPlugin(config.input_variable_setter())
              : std::make_unique<RequestsPerSecondInputVariableSetter>(
                    nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig())},
      is_exponential_phase_{true},
      exponential_factor_{config_.exponential_factor() > 0.0 ? config_.exponential_factor() : 2.0},
      previous_load_value_{std::numeric_limits<double>::signaling_NaN()},
      current_load_value_{config_.initial_value()},
      bottom_load_value_{std::numeric_limits<double>::signaling_NaN()},
      top_load_value_{std::numeric_limits<double>::signaling_NaN()} {}

nighthawk::client::CommandLineOptions
ExponentialSearchStepController::GetCurrentCommandLineOptions() const {
  nighthawk::client::CommandLineOptions options = command_line_options_template_;
  input_variable_setter_->SetInputVariable(&options, current_load_value_);
  return options;
}

bool ExponentialSearchStepController::IsConverged() const {
  // Binary search has brought successive input values within 1% of each other.
  return !is_exponential_phase_ && abs(current_load_value_ / previous_load_value_ - 1.0) < 0.01;
}

void ExponentialSearchStepController::UpdateAndRecompute(const BenchmarkResult& benchmark_result) {
  double score = TotalWeightedScore(benchmark_result);

  if (is_exponential_phase_) {
    if (score > 0.0) {
      // Have not reached the threshold yet; continue increasing the load exponentially.
      previous_load_value_ = current_load_value_;
      current_load_value_ *= config_.exponential_factor();
    } else {
      // We have found a value that exceeded the threshold.
      // Prepare for the binary search phase.
      is_exponential_phase_ = false;
      bottom_load_value_ = previous_load_value_;
      top_load_value_ = current_load_value_;
      previous_load_value_ = current_load_value_;
      current_load_value_ = (bottom_load_value_ + top_load_value_) / 2;
    }
  } else {
    // Binary search phase.
    if (score > 0.0) {
      // Within threshold, go higher.
      bottom_load_value_ = current_load_value_;
    } else {
      // Outside threshold, go lower.
      top_load_value_ = current_load_value_;
    }
    previous_load_value_ = current_load_value_;
    current_load_value_ = (bottom_load_value_ + top_load_value_) / 2;
  }
}

} // namespace AdaptiveLoad
} // namespace Nighthawk
