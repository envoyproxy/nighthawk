#include "nighthawk/adaptive_rps/custom_metric_evaluator.h"
#include "nighthawk/adaptive_rps/metrics_plugin.h"
#include "nighthawk/adaptive_rps/step_controller.h"

#include "external/envoy/source/common/config/utility.h"

namespace Nighthawk {
namespace AdaptiveRps {

CustomMetricEvaluatorPtr LoadCustomMetricEvaluatorPlugin(
    const nighthawk::adaptive_rps::CustomMetricEvaluatorConfig& config) {
  CustomMetricEvaluatorConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<CustomMetricEvaluatorConfigFactory>(
          config.name());
  return config_factory.createCustomMetricEvaluator(config.typed_config());
}

MetricsPluginPtr LoadMetricsPlugin(const nighthawk::adaptive_rps::MetricsPluginConfig& config) {
  MetricsPluginConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<MetricsPluginConfigFactory>(config.name());
  return config_factory.createMetricsPlugin(config.typed_config());
}

StepControllerPtr
LoadStepControllerPlugin(const nighthawk::adaptive_rps::StepControllerConfig& config) {
  StepControllerConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<StepControllerConfigFactory>(config.name());
  return config_factory.createStepController(config.typed_config());
}

} // namespace AdaptiveRps
} // namespace Nighthawk
