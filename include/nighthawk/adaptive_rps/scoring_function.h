#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/typed_config.h"

#include "api/adaptive_rps/scoring_function.pb.h"

namespace Nighthawk {
namespace AdaptiveRps {

// An interface for custom functions that measure a metric relative to a threshold.
class ScoringFunction {
public:
  virtual ~ScoringFunction() = default;
  // Returns a value between -1.0 and 1.0: 1.0 means the metric value is highly favorable and a
  // large RPS increase should be attempted. -1.0 means the metric value is highly unfavorable and a
  // large RPS decrease is needed. 0.0 means the metric is exactly at the threshold.
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
  virtual ScoringFunctionPtr createScoringFunction(const Envoy::Protobuf::Message& config_any) = 0;
};

} // namespace AdaptiveRps
} // namespace Nighthawk
