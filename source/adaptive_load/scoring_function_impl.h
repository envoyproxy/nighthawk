#pragma once

#include "nighthawk/adaptive_load/scoring_function.h"

#include "api/adaptive_load/scoring_function_impl.pb.h"

namespace Nighthawk {

// ScoringFunction that returns 1.0 when a metric is within thresholds and -1.0 otherwise.
// Supports upper or lower threshold or both.
class BinaryScoringFunction : public ScoringFunction {
public:
  explicit BinaryScoringFunction(
      const nighthawk::adaptive_load::BinaryScoringFunctionConfig& config);
  double EvaluateMetric(double value) const override;

private:
  // Upper threshold for the metric.
  double upper_threshold_;
  // Lower threshold for the metric.
  double lower_threshold_;
};

// Factory that creates a BinaryScoringFunction from a BinaryScoringFunctionConfig proto.
// Registered as an Envoy plugin.
class BinaryScoringFunctionConfigFactory : public ScoringFunctionConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  ScoringFunctionPtr createScoringFunction(const Envoy::Protobuf::Message& message) override;
};

// ScoringFunction that calculates a metric score as k * (threshold - value), where k is a scaling
// constant. The score is 0.0 when the value exactly equals the threshold, positive below the
// threshold (meaning load should increase), and negative above the threshold. The score is
// proportional to the difference from the threshold.
class LinearScoringFunction : public ScoringFunction {
public:
  explicit LinearScoringFunction(
      const nighthawk::adaptive_load::LinearScoringFunctionConfig& config);
  double EvaluateMetric(double value) const override;

private:
  // The target value of the metric.
  double threshold_;
  // Scaling constant: k in k * (threshold - value). Use this in combination
  // with step controller constants to produce reasonable load increments for
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
      const nighthawk::adaptive_load::SigmoidScoringFunctionConfig& config);
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

} // namespace Nighthawk
