#include <math.h>

#include "adaptive_rps/custom_metric_evaluator_impl.h"
#include "api/adaptive_rps/custom_metric_evaluator_impl.pb.h"
#include "envoy/registry/registry.h"

namespace Nighthawk {
namespace AdaptiveRps {

std::string SigmoidCustomMetricEvaluatorConfigFactory::name() const { return "sigmoid"; }

Envoy::ProtobufTypes::MessagePtr
SigmoidCustomMetricEvaluatorConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_rps::SigmoidCustomMetricEvaluatorConfig>();
}

CustomMetricEvaluatorPtr SigmoidCustomMetricEvaluatorConfigFactory::createCustomMetricEvaluator(
    const Envoy::Protobuf::Message& message) {
  const google::protobuf::Any& any = dynamic_cast<const google::protobuf::Any&>(message);
  nighthawk::adaptive_rps::SigmoidCustomMetricEvaluatorConfig config;
  any.UnpackTo(&config);
  return std::make_unique<SigmoidCustomMetricEvaluator>(config);
}

REGISTER_FACTORY(SigmoidCustomMetricEvaluatorConfigFactory, CustomMetricEvaluatorConfigFactory);

SigmoidCustomMetricEvaluator::SigmoidCustomMetricEvaluator(
    const nighthawk::adaptive_rps::SigmoidCustomMetricEvaluatorConfig& config)
    : threshold_{config.threshold()}, k_{config.k()} {}

double SigmoidCustomMetricEvaluator::EvaluateMetric(double value) const {
  return 1.0 - 2.0 / (1.0 + exp(-k_ * (value - threshold_)));
}

}  // namespace AdaptiveRps
}  // namespace Nighthawk
