#pragma once

#include "nighthawk/adaptive_load/input_variable_setter.h"
#include "nighthawk/adaptive_load/metrics_plugin.h"
#include "nighthawk/adaptive_load/scoring_function.h"
#include "nighthawk/adaptive_load/step_controller.h"

namespace Nighthawk {
namespace AdaptiveLoad {

// Instantiates an InputVariableSetter plugin based on the plugin name in |config|, unpacking the
// plugin-specific config proto within |config|. Throws Envoy::EnvoyException if the plugin is not
// found.
InputVariableSetterPtr
LoadInputVariableSetterPlugin(const nighthawk::adaptive_load::InputVariableSetterConfig& config);

// Instantiates a ScoringFunction plugin based on the plugin name in |config|, unpacking the
// plugin-specific config proto within |config|. Throws Envoy::EnvoyException if the plugin is not
// found.
ScoringFunctionPtr
LoadScoringFunctionPlugin(const nighthawk::adaptive_load::ScoringFunctionConfig& config);

// Instantiates a MetricsPlugin based on the plugin name in |config|, unpacking the
// plugin-specific config proto within |config|. Throws Envoy::EnvoyException if the plugin is not
// found.
MetricsPluginPtr LoadMetricsPlugin(const nighthawk::adaptive_load::MetricsPluginConfig& config);

// Instantiates a StepController plugin based on the plugin name in |config|, unpacking the
// plugin-specific config proto within |config|. Also requires the CommandLineOptions traffic
// template from the AdaptiveLoadSessionSpec. Throws Envoy::EnvoyException if the plugin is not
// found.
StepControllerPtr LoadStepControllerPlugin(
    const nighthawk::adaptive_load::StepControllerConfig& config,
    const nighthawk::client::CommandLineOptions& command_line_options_template);

} // namespace AdaptiveLoad
} // namespace Nighthawk
