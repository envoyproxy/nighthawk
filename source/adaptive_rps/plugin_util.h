#pragma once

#include "nighthawk/adaptive_rps/custom_metric_evaluator.h"
#include "nighthawk/adaptive_rps/metrics_plugin.h"
#include "nighthawk/adaptive_rps/step_controller.h"

namespace Nighthawk {
namespace AdaptiveRps {

// Instantiates a CustomMetricEvaluator plugin based on the plugin name in |config|, unpacking the
// plugin-specific config proto within |config|. Throws Envoy::EnvoyException if the plugin is not
// found.
CustomMetricEvaluatorPtr
LoadCustomMetricEvaluatorPlugin(const nighthawk::adaptive_rps::CustomMetricEvaluatorConfig& config);

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
