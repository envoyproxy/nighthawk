#include "nighthawk/adaptive_rps/metrics_plugin.h"
#include "nighthawk/adaptive_rps/scoring_function.h"
#include "nighthawk/adaptive_rps/step_controller.h"

#include "external/envoy/source/common/config/utility.h"

namespace Nighthawk {
namespace AdaptiveRps {

ScoringFunctionPtr
LoadScoringFunctionPlugin(const nighthawk::adaptive_rps::ScoringFunctionConfig& config) {
  ScoringFunctionConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<ScoringFunctionConfigFactory>(config.name());
  return config_factory.createScoringFunction(config.typed_config());
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
