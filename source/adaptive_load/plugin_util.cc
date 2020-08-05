#include "envoy/config/core/v3/base.pb.h"

#include "nighthawk/adaptive_load/input_variable_setter.h"
#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "nighthawk/adaptive_load/scoring_function.h"
#include "nighthawk/adaptive_load/step_controller.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/config/utility.h"

namespace Nighthawk {

Envoy::StatusOr<InputVariableSetterPtr>
LoadInputVariableSetterPlugin(const envoy::config::core::v3::TypedExtensionConfig& config) {
  try {
    InputVariableSetterConfigFactory& config_factory =
        Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
            config.name());
    absl::Status validation_status = config_factory.ValidateConfig(config.typed_config());
    if (!validation_status.ok()) {
      return validation_status;
    }
    return config_factory.createInputVariableSetter(config.typed_config());
  } catch (const Envoy::EnvoyException& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Could not load plugin: ", config.DebugString(), ": ", e.what()));
  }
}

Envoy::StatusOr<ScoringFunctionPtr>
LoadScoringFunctionPlugin(const envoy::config::core::v3::TypedExtensionConfig& config) {
  try {
    ScoringFunctionConfigFactory& config_factory =
        Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(
            config.name());
    absl::Status validation_status = config_factory.ValidateConfig(config.typed_config());
    if (!validation_status.ok()) {
      return validation_status;
    }
    return config_factory.createScoringFunction(config.typed_config());
  } catch (const Envoy::EnvoyException& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Could not load plugin: ", config.DebugString(), ": ", e.what()));
  }
}

Envoy::StatusOr<MetricsPluginPtr>
LoadMetricsPlugin(const envoy::config::core::v3::TypedExtensionConfig& config) {
  try {
    MetricsPluginConfigFactory& config_factory =
        Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(config.name());
    absl::Status validation_status = config_factory.ValidateConfig(config.typed_config());
    if (!validation_status.ok()) {
      return validation_status;
    }
    return config_factory.createMetricsPlugin(config.typed_config());
  } catch (const Envoy::EnvoyException& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Could not load plugin: ", config.DebugString(), ": ", e.what()));
  }
}

Envoy::StatusOr<StepControllerPtr> LoadStepControllerPlugin(
    const envoy::config::core::v3::TypedExtensionConfig& config,
    const nighthawk::client::CommandLineOptions& command_line_options_template) {
  try {
    StepControllerConfigFactory& config_factory =
        Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(
            config.name());
    absl::Status validation_status = config_factory.ValidateConfig(config.typed_config());
    if (!validation_status.ok()) {
      return validation_status;
    }
    return config_factory.createStepController(config.typed_config(),
                                               command_line_options_template);
  } catch (const Envoy::EnvoyException& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Could not load plugin: ", config.DebugString(), ": ", e.what()));
  }
}

} // namespace Nighthawk
