#pragma once

#include "envoy/config/core/v3/base.pb.h"

#include "nighthawk/adaptive_load/input_variable_setter.h"
#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "nighthawk/adaptive_load/scoring_function.h"
#include "nighthawk/adaptive_load/step_controller.h"

namespace Nighthawk {

/**
 * Instantiates an InputVariableSetter plugin based on the plugin name in |config|, unpacking the
 * plugin-specific config proto within |config|. Validates the config proto.
 *
 * @param config Proto containing plugin name and plugin-specific config proto.
 *
 * @return absl::StatusOr<InputVariableSetterPtr> Initialized plugin or error status due to missing
 * plugin or config proto validation error.
 */
absl::StatusOr<InputVariableSetterPtr>
LoadInputVariableSetterPlugin(const envoy::config::core::v3::TypedExtensionConfig& config);

/**
 * Instantiates a ScoringFunction plugin based on the plugin name in |config|, unpacking the
 * plugin-specific config proto within |config|. Validates the config proto.
 *
 * @param config Proto containing plugin name and plugin-specific config proto.
 *
 * @return absl::StatusOr<ScoringFunctionPtr> Initialized plugin or error status due to missing
 * plugin or config proto validation error.
 */
absl::StatusOr<ScoringFunctionPtr>
LoadScoringFunctionPlugin(const envoy::config::core::v3::TypedExtensionConfig& config);

/**
 * Instantiates a MetricsPlugin based on the plugin name in |config|, unpacking the
 * plugin-specific config proto within |config|. Validates the config proto.
 *
 * @param config Proto containing plugin name and plugin-specific config proto.
 *
 * @return absl::StatusOr<MetricsPluginPtr> Initialized plugin or error status due to missing
 * plugin or config proto validation error.
 */
absl::StatusOr<MetricsPluginPtr>
LoadMetricsPlugin(const envoy::config::core::v3::TypedExtensionConfig& config);

/**
 * Instantiates a StepController plugin based on the plugin name in |config|, unpacking the
 * plugin-specific config proto within |config|. Validates the config proto.
 *
 * @param config Proto containing plugin name and plugin-specific config proto.
 * @param command_line_options_template CommandLineOptions traffic template from the
 * AdaptiveLoadSessionSpec.
 *
 * @return absl::StatusOr<StepControllerPtr> Initialized plugin or error status due to missing
 * plugin or config proto validation error.
 */
absl::StatusOr<StepControllerPtr> LoadStepControllerPlugin(
    const envoy::config::core::v3::TypedExtensionConfig& config,
    const nighthawk::client::CommandLineOptions& command_line_options_template);

} // namespace Nighthawk
