#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/core/v3/base.pb.h"
#include "envoy/config/typed_config.h"

namespace Nighthawk {
namespace AdaptiveLoad {

// An interface for custom functions that measure a metric relative to a threshold.
class ScoringFunction {
public:
  virtual ~ScoringFunction() = default;
  // Returns a score of 0.0 if the metric is exactly the threshold, a positive score if the metric
  // is below the threshold and load should be increased, and a negative score if the metric is
  // above the threshold and load should be decreased.
  virtual double EvaluateMetric(double value) const PURE;
};

using ScoringFunctionPtr = std::unique_ptr<ScoringFunction>;

// A factory that must be implemented for each ScoringFunction plugin. It instantiates the
// specific ScoringFunction class after unpacking the plugin-specific config proto.
class ScoringFunctionConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~ScoringFunctionConfigFactory() override = default;
  std::string category() const override { return "nighthawk.scoring_function"; }
  // Instantiates the specific ScoringFunction class. Casts |message| to Any, unpacks it to
  // the plugin-specific proto, and passes the strongly typed proto to the constructor.
  virtual ScoringFunctionPtr createScoringFunction(const Envoy::Protobuf::Message& message) PURE;
};

} // namespace AdaptiveLoad
} // namespace Nighthawk
