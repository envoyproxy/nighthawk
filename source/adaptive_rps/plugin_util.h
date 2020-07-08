#pragma once

#include "nighthawk/adaptive_rps/metrics_plugin.h"
#include "nighthawk/adaptive_rps/scoring_function.h"
#include "nighthawk/adaptive_rps/step_controller.h"

namespace Nighthawk {
namespace AdaptiveRps {

// Instantiates a ScoringFunction plugin based on the plugin name in |config|, unpacking the
// plugin-specific config proto within |config|. Throws Envoy::EnvoyException if the plugin is not
// found.
ScoringFunctionPtr
LoadScoringFunctionPlugin(const nighthawk::adaptive_rps::ScoringFunctionConfig& config);

// Instantiates a MetricsPlugin based on the plugin name in |config|, unpacking the
// plugin-specific config proto within |config|. Throws Envoy::EnvoyException if the plugin is not
// found.
MetricsPluginPtr LoadMetricsPlugin(const nighthawk::adaptive_rps::MetricsPluginConfig& config);

// Instantiates a StepController plugin based on the plugin name in |config|, unpacking the
// plugin-specific config proto within |config|. Throws Envoy::EnvoyException if the plugin is not
// found.
StepControllerPtr
LoadStepControllerPlugin(const nighthawk::adaptive_rps::StepControllerConfig& config);

} // namespace AdaptiveRps
} // namespace Nighthawk
