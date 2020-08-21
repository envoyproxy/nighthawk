#pragma once

#include "nighthawk/adaptive_load/input_variable_setter.h"
#include "nighthawk/adaptive_load/step_controller.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/step_controller_impl.pb.h"
#include "envoy/registry/registry.h"

namespace Nighthawk {

/**
 * A StepController that performs an exponential search for the highest load that keeps metrics
 * within thresholds. See https://en.wikipedia.org/wiki/Exponential_search.
 */
class ExponentialSearchStepController : public StepController {
public:
  explicit ExponentialSearchStepController(
      const nighthawk::adaptive_load::ExponentialSearchStepControllerConfig& config,
      nighthawk::client::CommandLineOptions command_line_options_template);
  absl::StatusOr<nighthawk::client::CommandLineOptions>
  GetCurrentCommandLineOptions() const override;
  bool IsConverged() const override;
  bool IsDoomed(std::string& doom_reason) const override;
  void UpdateAndRecompute(const nighthawk::adaptive_load::BenchmarkResult& result) override;

private:
  // Proto defining the traffic request to be sent to Nighthawk, apart from what is set by the
  // InputVariableSetter.
  const nighthawk::client::CommandLineOptions command_line_options_template_;
  // A plugin that applies a numerical load value to the traffic definition, e.g by setting
  // requests_per_second.
  InputVariableSetterPtr input_variable_setter_;
  // Whether the algorithm is in the exponential stage, as opposed to the subsequent binary search
  // stage.
  bool is_exponential_phase_;
  // The factor for increasing the load value in each recalculation during the exponential stage.
  double exponential_factor_;
  // The previous load the controller recommended before the most recent recalculation. NaN
  // initially.
  double previous_load_value_;
  // The load the controller will currently recommend, until the next recalculation.
  double current_load_value_;
  // The current bottom of the search range during the binary search stage.
  double bottom_load_value_;
  // The current top of the search range during the binary search stage.
  double top_load_value_;
  // Set when an error has been detected; exposed via IsDoomed().
  std::string doom_reason_;
};

/**
 * Factory that creates an ExponentialSearchStepController from an
 * ExponentialSearchStepControllerConfig proto. Registered as an Envoy plugin.
 */
class ExponentialSearchStepControllerConfigFactory : public StepControllerConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  absl::Status ValidateConfig(const Envoy::Protobuf::Message& config) const override;
  StepControllerPtr createStepController(
      const Envoy::Protobuf::Message& config,
      const nighthawk::client::CommandLineOptions& command_line_options_template) override;
};

// This factory is activated through LoadStepControllerPlugin in plugin_util.h.
DECLARE_FACTORY(ExponentialSearchStepControllerConfigFactory);

} // namespace Nighthawk
