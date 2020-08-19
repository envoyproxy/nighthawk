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
 * within thresholds.
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
  const nighthawk::client::CommandLineOptions command_line_options_template_;
  InputVariableSetterPtr input_variable_setter_;
  bool is_exponential_phase_;
  double exponential_factor_;
  double previous_load_value_;
  double current_load_value_;
  double bottom_load_value_;
  double top_load_value_;
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
