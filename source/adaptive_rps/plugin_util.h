#pragma once

#include "nighthawk/adaptive_rps/custom_metric_evaluator.h"
#include "nighthawk/adaptive_rps/metrics_plugin.h"
#include "nighthawk/adaptive_rps/step_controller.h"

namespace Nighthawk {
namespace AdaptiveRps {

CustomMetricEvaluatorPtr
LoadCustomMetricEvaluatorPlugin(const nighthawk::adaptive_rps::CustomMetricEvaluatorConfig& config);

MetricsPluginPtr LoadMetricsPlugin(const nighthawk::adaptive_rps::MetricsPluginConfig& config);

StepControllerPtr
LoadStepControllerPlugin(const nighthawk::adaptive_rps::StepControllerConfig& config);

}  // namespace AdaptiveRps
}  // namespace Nighthawk
