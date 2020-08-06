// Interfaces for StepController plugins and plugin factories.

#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "nighthawk/adaptive_load/config_validator.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_load/benchmark_result.pb.h"
#include "external/envoy/source/common/common/statusor.h"

namespace Nighthawk {

/**
 * An interface for StepControllers that compute load adjustments and check for convergence.
 *
 * See source/adaptive_load/step_controller_impl.h for example plugins.
 */
class StepController {
public:
  virtual ~StepController() = default;
  /**
   * Returns the current CommandLineOptions load specification that the StepController recommends.
   *
   * @return StatusOr<CommandLineOptions> The final product after applying all computed load
   * variables via InputVariableSetter plugins to the stored CommandLineOptions template, or an
   * error if the variables could not be applied (e.g. out of range).
   */
  virtual Envoy::StatusOr<nighthawk::client::CommandLineOptions>
  GetCurrentCommandLineOptions() const PURE;
  /**
   * Reports if the search for the optimal load has converged, based on the StepController's
   * internal state variables.
   *
   * @return bool Whether the load has converged.
   */
  virtual bool IsConverged() const PURE;
  /** Reports if the algorithm has determined it can never succeed as configured, e.g. because
   * metrics were outside thresholds at input values throughout the configured search range.
   *
   * @param doom_reason A string to store the reason for being doomed. If returning true, an
   * explanation of why success is impossible is written here; otherwise this string is not
   * touched.
   *
   * @return bool true if the controller has determined convergence is impossible.
   */
  virtual bool IsDoomed(std::string& doom_reason) const PURE;
  /**
   * Reports the result of the latest Nighthawk benchmark to the StepController so that the
   * StepController can add data to its history (if any), recompute any internal state, and
   * recompute its load recommendation.
   *
   * @param result The result of running a benchmark with Nighthawk Service, calling any
   * MetricsPlugins, and scoring all metrics against configured thresholds. Some StepController
   * plugins will store this value in a history internally.
   */
  virtual void UpdateAndRecompute(const nighthawk::adaptive_load::BenchmarkResult& result) PURE;
};

using StepControllerPtr = std::unique_ptr<StepController>;

/**
 * A factory that must be implemented for each StepController plugin. It instantiates the
 * specific StepController class after unpacking the plugin-specific config proto.
 */
class StepControllerConfigFactory : public Envoy::Config::TypedFactory, public ConfigValidator {
public:
  ~StepControllerConfigFactory() override = default;
  std::string category() const override { return "nighthawk.step_controller"; }
  /**
   * Instantiates the specific StepController class. Casts |message| to Any, unpacks it to the
   * plugin-specific proto, and passes the strongly typed proto to the plugin constructor.
   *
   * @param message Any typed_config proto taken from the TypedExtensionConfig.
   * @param command_line_options_template A partially filled CommandLineOptions describing all
   * aspects of the traffic not managed by this StepController. While running, this StepController
   * will be asked repeatedly for a fully formed CommandLineOptions with some variables filled in
   * dynamically, and this proto template is the basis for all such protos.
   *
   * @return StepControllerPtr Pointer to the new plugin instance.
   *
   * @throw Envoy::EnvoyException If the Any proto cannot be unpacked as the type expected by the
   * plugin.
   */
  virtual StepControllerPtr createStepController(
      const Envoy::Protobuf::Message& message,
      const nighthawk::client::CommandLineOptions& command_line_options_template) PURE;
};

} // namespace Nighthawk
