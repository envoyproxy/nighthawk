#include "adaptive_load/step_controller_impl.h"

#include <memory>

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/metric_spec.pb.h"
#include "api/adaptive_load/step_controller_impl.pb.h"

#include "adaptive_load/input_variable_setter_impl.h"
#include "adaptive_load/plugin_util.h"

namespace Nighthawk {

namespace {

using nighthawk::adaptive_load::BenchmarkResult;
using nighthawk::adaptive_load::ExponentialSearchStepControllerConfig;
using nighthawk::adaptive_load::MetricEvaluation;

// Adds all collected metric results according to their weights. Informational metrics will be
// weight 0.0.
double TotalWeightedScore(const BenchmarkResult& benchmark_result) {
  double score = 0.0;
  double total_weight = 0.0;
  for (const MetricEvaluation& evaluation : benchmark_result.metric_evaluations()) {
    score += evaluation.weight() * evaluation.threshold_score();
    total_weight += evaluation.weight();
  }
  return score / total_weight;
}

} // namespace

Envoy::ProtobufTypes::MessagePtr
ExponentialSearchStepControllerConfigFactory::createEmptyConfigProto() {
  return std::make_unique<ExponentialSearchStepControllerConfig>();
}

std::string ExponentialSearchStepControllerConfigFactory::name() const {
  return "nighthawk.exponential-search";
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
    : command_line_options_template_{command_line_options_template}, is_exponential_phase_{true},
      exponential_factor_{config.exponential_factor() > 0.0 ? config.exponential_factor() : 2.0},
      previous_load_value_{std::numeric_limits<double>::signaling_NaN()},
      current_load_value_{config.initial_value()},
      bottom_load_value_{std::numeric_limits<double>::signaling_NaN()},
      top_load_value_{std::numeric_limits<double>::signaling_NaN()} {
  doom_reason_ = "";
  if (config.has_input_variable_setter()) {
    try {
      input_variable_setter_ = LoadInputVariableSetterPlugin(config.input_variable_setter());
    } catch (const Envoy::EnvoyException& e) {
      doom_reason_ = absl::StrCat("Error loading plugin ",
                                  config.input_variable_setter().DebugString(), ": ", e.what());
    }
  } else {
    input_variable_setter_ = std::make_unique<RequestsPerSecondInputVariableSetter>(
        nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig());
  }
}

nighthawk::client::CommandLineOptions
ExponentialSearchStepController::GetCurrentCommandLineOptions() const {
  nighthawk::client::CommandLineOptions options = command_line_options_template_;
  input_variable_setter_->SetInputVariable(options, current_load_value_);
  return options;
}

bool ExponentialSearchStepController::IsConverged() const {
  // Binary search has brought successive input values within 1% of each other.
  return doom_reason_ == "" && !is_exponential_phase_ &&
         abs(current_load_value_ / previous_load_value_ - 1.0) < 0.01;
}

bool ExponentialSearchStepController::IsDoomed(std::string* doom_reason) const {
  if (doom_reason_ == "") {
    return false;
  } else {
    *doom_reason = doom_reason_;
    return true;
  }
}

void ExponentialSearchStepController::UpdateAndRecompute(const BenchmarkResult& benchmark_result) {
  double score = TotalWeightedScore(benchmark_result);

  if (is_exponential_phase_) {
    if (score > 0.0) {
      // Have not reached the threshold yet; continue increasing the load exponentially.
      previous_load_value_ = current_load_value_;
      current_load_value_ *= exponential_factor_;
    } else {
      // We have found a value that exceeded the threshold.
      // Prepare for the binary search phase.
      if (std::isnan(previous_load_value_)) {
        // Cannot continue if the initial value already exceeds metric thresholds.
        doom_reason_ = "Outside threshold on initial input";
        return;
      }
      is_exponential_phase_ = false;
      // Binary search between previous load (ok) and current load (too high).
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

} // namespace Nighthawk
