#pragma once

#include "envoy/registry/registry.h"

#include "nighthawk/adaptive_load/step_controller.h"

#include "api/client/options.pb.h"
#include "api/client/service.grpc.pb.h"

#include "test/adaptive_load/fake_plugins/fake_step_controller/fake_step_controller.pb.h"

namespace Nighthawk {

/**
 * StepController for testing: Configurable convergence and doom countdowns, fixed RPS value.
 */
class FakeStepController : public StepController {
public:
  /**
   * Initializes the fake step controller with a FakeStepControllerConfig proto.
   *
   * @param config FakeStepControllerConfig proto for setting the fixed RPS value.
   * @param command_line_options_template A template for producing Nighthawk input.
   */
  FakeStepController(nighthawk::adaptive_load::FakeStepControllerConfig config,
                     nighthawk::client::CommandLineOptions command_line_options_template);
  /**
   * @return bool The current value of |is_converged_|.
   */
  bool IsConverged() const override;
  /**
   * Returns |is_doomed_|, writing |doomed_reason_| to |doomed_reason| if |is_doomed_| is true.
   *
   * @param doomed_reason String to store the doom reason, written only if |is_doomed_| is true.
   *
   * @return bool The current value of |is_doomed_|.
   */
  bool IsDoomed(std::string& doomed_reason) const override;
  /**
   * @return int The value |fixed_rps_value_|.
   */
  absl::StatusOr<nighthawk::client::CommandLineOptions>
  GetCurrentCommandLineOptions() const override;
  /**
   * Updates |is_converged_| to reflect whether |benchmark_result| contains any score >0. Updates
   * |is_doomed_| to reflect whether |benchmark_result| contains any score <0. A non-converged,
   * non-doomed input has scores all equal to 0.
   *
   * @param benchmark_result A Nighthawk benchmark result proto.
   */
  void
  UpdateAndRecompute(const nighthawk::adaptive_load::BenchmarkResult& benchmark_result) override;

private:
  const nighthawk::adaptive_load::FakeStepControllerConfig config_;
  bool is_converged_;
  bool is_doomed_;
  std::string doomed_reason_;
  const nighthawk::client::CommandLineOptions command_line_options_template_;
};

/**
 * Factory that creates a FakeStepController plugin from a FakeStepControllerConfig proto.
 * Registered as an Envoy plugin.
 */
class FakeStepControllerConfigFactory : public StepControllerConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  StepControllerPtr createStepController(
      const Envoy::Protobuf::Message& config_any,
      const nighthawk::client::CommandLineOptions& command_line_options_template) override;
  absl::Status ValidateConfig(const Envoy::Protobuf::Message& message) const override;
};

// This factory is activated through LoadStepControllerPlugin in plugin_util.h.
DECLARE_FACTORY(FakeStepControllerConfigFactory);

/**
 * Creates a valid TypedExtensionConfig proto that activates a FakeStepController.
 *
 * @param fixed_rps_value Value for RPS to set in the FakeStepControllerConfig proto.
 *
 * @return TypedExtensionConfig A proto that activates FakeStepController by name and provides a
 * FakeStepControllerConfig proto wrapped in an Any.
 */
envoy::config::core::v3::TypedExtensionConfig
MakeFakeStepControllerPluginConfig(int fixed_rps_value);

/**
 * Creates a valid TypedExtensionConfig proto that activates a FakeStepController with a
 * FakeInputVariableSetterConfig that fails validation.
 *
 * @param artificial_validation_error An error status.
 *
 * @return TypedExtensionConfig A proto that activates FakeStepController by name and includes
 * a FakeStepControllerConfig proto wrapped in an Any. This proto will fail validation when
 * attempting to load the plugin.
 */
envoy::config::core::v3::TypedExtensionConfig MakeFakeStepControllerPluginConfigWithValidationError(
    const absl::Status& artificial_validation_error);

/**
 * Creates a valid TypedExtensionConfig proto that activates a FakeStepController with a
 * FakeInputVariableSetterConfig that returns an error from GetCurrentCommandLineOptions().
 *
 * @param artificial_input_setting_failure An error status.
 *
 * @return TypedExtensionConfig A proto that activates FakeStepController by name and includes
 * a FakeStepControllerConfig proto wrapped in an Any.
 */
envoy::config::core::v3::TypedExtensionConfig
MakeFakeStepControllerPluginConfigWithInputSettingError(
    const absl::Status& artificial_input_setting_failure);

} // namespace Nighthawk
