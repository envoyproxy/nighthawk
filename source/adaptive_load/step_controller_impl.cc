#include "source/adaptive_load/step_controller_impl.h"

#include <memory>

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/metric_spec.pb.h"
#include "api/adaptive_load/step_controller_impl.pb.h"

#include "source/adaptive_load/input_variable_setter_impl.h"
#include "source/adaptive_load/plugin_loader.h"

namespace Nighthawk {

namespace {

using ::nighthawk::adaptive_load::BenchmarkResult;
using ::nighthawk::adaptive_load::ExponentialSearchStepControllerConfig;
using ::nighthawk::adaptive_load::MetricEvaluation;

/**
 * Checks if any non-informational metrics (weight > 0) were outside thresholds (score < 0).
 *
 * @param benchmark_result Metrics from the latest Nighthawk benchmark session.
 *
 * @return double -1.0 if any metric was outside its threshold or 1.0 if all metrics were within
 * thresholds.
 */
double TotalScore(const BenchmarkResult& benchmark_result) {
  for (const MetricEvaluation& evaluation : benchmark_result.metric_evaluations()) {
    if (evaluation.weight() > 0.0 && evaluation.threshold_score() < 0.0) {
      return -1.0;
    }
  }
  return 1.0;
}

} // namespace

Envoy::ProtobufTypes::MessagePtr
ExponentialSearchStepControllerConfigFactory::createEmptyConfigProto() {
  return std::make_unique<ExponentialSearchStepControllerConfig>();
}

std::string ExponentialSearchStepControllerConfigFactory::name() const {
  return "nighthawk.exponential_search";
}

absl::Status ExponentialSearchStepControllerConfigFactory::ValidateConfig(
    const Envoy::Protobuf::Message& message) const {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  ExponentialSearchStepControllerConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  if (config.has_input_variable_setter()) {
    return LoadInputVariableSetterPlugin(config.input_variable_setter()).status();
  }
  return absl::OkStatus();
}

StepControllerPtr ExponentialSearchStepControllerConfigFactory::createStepController(
    const Envoy::Protobuf::Message& message,
    const nighthawk::client::CommandLineOptions& command_line_options_template) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  ExponentialSearchStepControllerConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<ExponentialSearchStepController>(config, command_line_options_template);
}

REGISTER_FACTORY(ExponentialSearchStepControllerConfigFactory, StepControllerConfigFactory);

ExponentialSearchStepController::ExponentialSearchStepController(
    const ExponentialSearchStepControllerConfig& config,
    nighthawk::client::CommandLineOptions command_line_options_template)
    : command_line_options_template_{std::move(command_line_options_template)},
      exponential_factor_{config.exponential_factor() > 0.0 ? config.exponential_factor() : 2.0},
      current_load_value_{config.initial_value()} {
  doom_reason_ = "";
  if (config.has_input_variable_setter()) {
    absl::StatusOr<InputVariableSetterPtr> input_variable_setter_or =
        LoadInputVariableSetterPlugin(config.input_variable_setter());
    RELEASE_ASSERT(input_variable_setter_or.ok(),
                   absl::StrCat("InputVariableSetter plugin loading error should have been caught "
                                "during input validation: ",
                                input_variable_setter_or.status().message()));
    input_variable_setter_ = std::move(input_variable_setter_or.value());
  } else {
    input_variable_setter_ = std::make_unique<RequestsPerSecondInputVariableSetter>(
        nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig());
  }
}

absl::StatusOr<nighthawk::client::CommandLineOptions>
ExponentialSearchStepController::GetCurrentCommandLineOptions() const {
  nighthawk::client::CommandLineOptions options = command_line_options_template_;
  absl::Status status = input_variable_setter_->SetInputVariable(options, current_load_value_);
  if (!status.ok()) {
    return status;
  }
  return options;
}

bool ExponentialSearchStepController::IsConverged() const {
  // Binary search has brought successive input values within 1% of each other.
  return doom_reason_.empty() && !is_range_finding_phase_ &&
         abs(current_load_value_ / previous_load_value_ - 1.0) < 0.01;
}

bool ExponentialSearchStepController::IsDoomed(std::string& doom_reason) const {
  if (doom_reason_.empty()) {
    return false;
  }
  doom_reason = doom_reason_;
  return true;
}

void ExponentialSearchStepController::UpdateAndRecompute(const BenchmarkResult& benchmark_result) {
  const double score = TotalScore(benchmark_result);
  if (is_range_finding_phase_) {
    IterateRangeFindingPhase(score);
  } else {
    IterateBinarySearchPhase(score);
  }
}

/**
 * Updates state variables based on the latest score. Exponentially increases the load in each step.
 * Transitions to the binary search phase when the load has caused metrics to go outside thresholds.
 */
void ExponentialSearchStepController::IterateRangeFindingPhase(double score) {
  if (score > 0.0) {
    // Have not reached the threshold yet; continue increasing the load exponentially.
    previous_load_value_ = current_load_value_;
    current_load_value_ *= exponential_factor_;
  } else {
    // We have found a value that exceeded the threshold.
    // Prepare for the binary search phase.
    if (std::isnan(previous_load_value_)) {
      doom_reason_ =
          "ExponentialSearchStepController cannot continue if the metrics values already exceed "
          "metric thresholds with the initial load. Check the initial load value in the "
          "ExponentialSearchStepControllerConfig, requested metrics, and thresholds.";
      return;
    }
    is_range_finding_phase_ = false;
    // Binary search is between previous load (ok) and current load (too high).
    bottom_load_value_ = previous_load_value_;
    top_load_value_ = current_load_value_;

    previous_load_value_ = current_load_value_;
    current_load_value_ = (bottom_load_value_ + top_load_value_) / 2;
  }
}

/**
 * Updates state variables based on the latest score. Performs one step of a binary search.
 */
void ExponentialSearchStepController::IterateBinarySearchPhase(double score) {
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

} // namespace Nighthawk
