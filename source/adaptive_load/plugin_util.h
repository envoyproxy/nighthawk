#pragma once

#include "envoy/config/core/v3/base.pb.h"

#include "nighthawk/adaptive_load/input_variable_setter.h"
#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "nighthawk/adaptive_load/scoring_function.h"
#include "nighthawk/adaptive_load/step_controller.h"

namespace Nighthawk {

/**
 * Instantiates an InputVariableSetter plugin based on the plugin name in |config|, unpacking the
 * plugin-specific config proto within |config|. Throws Envoy::EnvoyException if the plugin is not
 * found.
 *
 * @param config Proto containing plugin name and plugin-specific config proto.
 *
 * @return InputVariableSetterPtr Initialized plugin.
 */
InputVariableSetterPtr
LoadInputVariableSetterPlugin(const envoy::config::core::v3::TypedExtensionConfig& config);

/**
 * Instantiates a ScoringFunction plugin based on the plugin name in |config|, unpacking the
 * plugin-specific config proto within |config|. Throws Envoy::EnvoyException if the plugin is not
 * found.
 *
 * @param config Proto containing plugin name and plugin-specific config proto.
 *
 * @return ScoringFunctionPtr Initialized plugin.
 */
ScoringFunctionPtr
LoadScoringFunctionPlugin(const envoy::config::core::v3::TypedExtensionConfig& config);

/**
 * Instantiates a MetricsPlugin based on the plugin name in |config|, unpacking the
 * plugin-specific config proto within |config|. Throws Envoy::EnvoyException if the plugin is not
 * found.
 *
 * @param config Proto containing plugin name and plugin-specific config proto.
 *
 * @return MetricsPluginPtr Initialized plugin.
 */
MetricsPluginPtr LoadMetricsPlugin(const envoy::config::core::v3::TypedExtensionConfig& config);

/**
 * Instantiates a StepController plugin based on the plugin name in |config|, unpacking the
 * plugin-specific config proto within |config|. Throws Envoy::EnvoyException if the plugin is not
 * found.
 *
 * @param config Proto containing plugin name and plugin-specific config proto.
 * @param command_line_options_template CommandLineOptions traffic template from the
 * AdaptiveLoadSessionSpec.
 *
 * @return StepControllerPtr Initialized plugin.
 */
StepControllerPtr LoadStepControllerPlugin(
    const envoy::config::core::v3::TypedExtensionConfig& config,
    const nighthawk::client::CommandLineOptions& command_line_options_template);

} // namespace Nighthawk
