#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/adaptive_load/step_controller.pb.h"

namespace Nighthawk {
namespace AdaptiveLoad {

// An interface for custom StepControllers that compute load adjustments and check for convergence.
class StepController {
public:
  virtual ~StepController() = default;
  // Returns the current CommandLineOptions load specification that the StepController recommends.
  virtual nighthawk::client::CommandLineOptions GetCurrentCommandLineOptions() const PURE;
  // Determines whether the search for the optimal load has converged, based on the StepController's
  // internal state variables.
  virtual bool IsConverged() const PURE;
  // Reports the result of the latest Nighthawk benchmark to the StepController so that the
  // StepController can add data to its history (if any), recompute any internal state, and
  // recompute its load recommendation.
  virtual void UpdateAndRecompute(const nighthawk::adaptive_load::BenchmarkResult& result) PURE;
};

using StepControllerPtr = std::unique_ptr<StepController>;

// A factory that must be implemented for each StepController plugin. It instantiates the
// specific StepController class after unpacking the plugin-specific config proto.
class StepControllerConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~StepControllerConfigFactory() override = default;
  std::string category() const override { return "nighthawk.step_controller"; }
  // Instantiates the specific StepController class. Casts |message| to Any, unpacks it to the
  // plugin-specific proto, and passes the strongly typed proto to the constructor.
  virtual StepControllerPtr createStepController(
      const Envoy::Protobuf::Message& message,
      const nighthawk::client::CommandLineOptions& command_line_options_template) PURE;
};

} // namespace AdaptiveLoad
} // namespace Nighthawk
