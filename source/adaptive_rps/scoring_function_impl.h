#pragma once

#include "nighthawk/adaptive_rps/scoring_function.h"

#include "api/adaptive_rps/scoring_function_impl.pb.h"

namespace Nighthawk {
namespace AdaptiveRps {

// ScoringFunction that calculates a metric score as k * (threshold - value), where k is a scaling
// constant. The score is 0.0 when the value exactly equals the threshold, positive below the
// threshold (meaning RPS should increase), and negative above the threshold. The score is
// proportional to the difference from the threshold.
class LinearScoringFunction : public ScoringFunction {
public:
  explicit LinearScoringFunction(
      const nighthawk::adaptive_rps::LinearScoringFunctionConfig& config);
  double EvaluateMetric(double value) const override;

private:
  // The target value of the metric.
  double threshold_;
  // Scaling constant: k in k * (threshold - value). Use this in combination
  // with step controller constants to produce reasonable RPS increments for
  // reasonable differences from the threshold.
  double k_;
};

// Factory that creates a LinearScoringFunction from a LinearScoringFunctionConfig proto.
// Registered as an Envoy plugin.
class LinearScoringFunctionConfigFactory : public ScoringFunctionConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  ScoringFunctionPtr createScoringFunction(const Envoy::Protobuf::Message& message) override;
};

// Configuration for a ScoringFunction that calculates a metric score
// as 1 - 2 / (1 + exp(-k(value - threshold))), an upside-down sigmoid curve
// centered on a threshold. The output is 0.0 when the metric equals the
// threshold, approaches 1.0 for values far below the threshold, and approaches
// -1.0 for values far above the threshold.
class SigmoidScoringFunction : public ScoringFunction {
public:
  explicit SigmoidScoringFunction(
      const nighthawk::adaptive_rps::SigmoidScoringFunctionConfig& config);
  double EvaluateMetric(double value) const override;

private:
  // The target value of the metric.
  double threshold_;
  // Tuning constant: k in 1 - 2 / (1 + exp(-k(value - threshold))). k should
  // be around the same size as 1/threshold.
  double k_;
};

// Factory that creates a SigmoidScoringFunction from a SigmoidScoringFunctionConfig proto.
// Registered as an Envoy plugin.
class SigmoidScoringFunctionConfigFactory : public ScoringFunctionConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  ScoringFunctionPtr createScoringFunction(const Envoy::Protobuf::Message& message) override;
};

} // namespace AdaptiveRps
} // namespace Nighthawk
