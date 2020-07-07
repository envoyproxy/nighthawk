#pragma once

#include <string>

#include "api/adaptive_rps/benchmark_result.pb.h"
#include "api/adaptive_rps/step_controller.pb.h"
// #include "third_party/envoy/src/source/common/config/utility.h"
// #include "third_party/envoy/src/source/common/protobuf/protobuf.h"

#include "api/adaptive_rps/metrics_plugin.pb.h"
#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"
#include "envoy/registry/registry.h"

namespace Nighthawk {
namespace AdaptiveRps {

class StepController {
public:
  virtual ~StepController() = default;
  virtual unsigned int GetCurrentRps() const PURE;
  virtual bool IsConverged() const PURE;
  virtual void UpdateAndRecompute(const nighthawk::adaptive_rps::BenchmarkResult& result) PURE;
};

using StepControllerPtr = std::unique_ptr<StepController>;

class StepControllerConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~StepControllerConfigFactory() override = default;
  std::string category() const override { return "nighthawk.step_controller"; }
  virtual StepControllerPtr createStepController(const Envoy::Protobuf::Message&) PURE;
};

}  // namespace AdaptiveRps
}  // namespace Nighthawk
