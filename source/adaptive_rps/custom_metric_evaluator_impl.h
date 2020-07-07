#pragma once

#include "nighthawk/adaptive_rps/custom_metric_evaluator.h"

#include "api/adaptive_rps/custom_metric_evaluator_impl.pb.h"

namespace Nighthawk {
namespace AdaptiveRps {

class SigmoidCustomMetricEvaluatorConfigFactory : public CustomMetricEvaluatorConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  CustomMetricEvaluatorPtr
  createCustomMetricEvaluator(const Envoy::Protobuf::Message& message) override;
};

class SigmoidCustomMetricEvaluator : public CustomMetricEvaluator {
public:
  explicit SigmoidCustomMetricEvaluator(
      const nighthawk::adaptive_rps::SigmoidCustomMetricEvaluatorConfig& config);
  double EvaluateMetric(double value) const override;

private:
  double threshold_;
  double k_;
};

} // namespace AdaptiveRps
} // namespace Nighthawk
