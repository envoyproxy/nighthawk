// Interfaces for ScoringFunction plugins and plugin factories.

#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "nighthawk/adaptive_load/config_validator.h"

namespace Nighthawk {

/**
 * An interface for custom functions that score a metric relative to a threshold.
 *
 * See source/adaptive_load/scoring_function_impl.h for example plugins.
 */
class ScoringFunction {
public:
  virtual ~ScoringFunction() = default;
  /**
   * Returns a score of 0.0 if the metric is exactly the threshold, a positive score if the metric
   * is below the threshold and load should be increased, and a negative score if the metric is
   * above the threshold and load should be decreased. The magnitude of the value is determined in a
   * plugin-specific way, based on thresholds and other configuration.
   *
   * @param value The measurement to be scored.
   *
   * @return double The score of the measurement according to the formula of this plugin.
   */
  virtual double EvaluateMetric(double value) const PURE;
};

using ScoringFunctionPtr = std::unique_ptr<ScoringFunction>;

/**
 * A factory that must be implemented for each ScoringFunction plugin. It instantiates the
 * specific ScoringFunction class after unpacking the plugin-specific config proto.
 */
class ScoringFunctionConfigFactory : public virtual Envoy::Config::TypedFactory,
                                     public virtual ConfigValidator {
public:
  std::string category() const override { return "nighthawk.scoring_function"; }
  /**
   * Instantiates the specific ScoringFunction class. Casts |message| to Any, unpacks it to the
   * plugin-specific proto, and passes the strongly typed proto to the plugin constructor.
   *
   * @param message Any typed_config proto taken from the TypedExtensionConfig.
   *
   * @return ScoringFunctionPtr Pointer to the new plugin instance.
   *
   * @throw Envoy::EnvoyException If the Any proto cannot be unpacked as the type expected by the
   * plugin.
   */
  virtual ScoringFunctionPtr createScoringFunction(const Envoy::Protobuf::Message& message) PURE;
};

} // namespace Nighthawk
