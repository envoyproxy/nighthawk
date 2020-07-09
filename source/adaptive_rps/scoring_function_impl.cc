#include "adaptive_rps/scoring_function_impl.h"

#include <math.h>

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_rps/scoring_function_impl.pb.h"

namespace Nighthawk {
namespace AdaptiveRps {

std::string LinearScoringFunctionConfigFactory::name() const { return "linear"; }

Envoy::ProtobufTypes::MessagePtr LinearScoringFunctionConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_rps::LinearScoringFunctionConfig>();
}

ScoringFunctionPtr
LinearScoringFunctionConfigFactory::createScoringFunction(const Envoy::Protobuf::Message& message) {
  const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_rps::LinearScoringFunctionConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<LinearScoringFunction>(config);
}

REGISTER_FACTORY(LinearScoringFunctionConfigFactory, ScoringFunctionConfigFactory);

LinearScoringFunction::LinearScoringFunction(
    const nighthawk::adaptive_rps::LinearScoringFunctionConfig& config)
    : threshold_{config.threshold()}, k_{config.k()} {}

double LinearScoringFunction::EvaluateMetric(double value) const {
  return k_ * (threshold_ - value);
}

std::string SigmoidScoringFunctionConfigFactory::name() const { return "sigmoid"; }

Envoy::ProtobufTypes::MessagePtr SigmoidScoringFunctionConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_rps::SigmoidScoringFunctionConfig>();
}

ScoringFunctionPtr SigmoidScoringFunctionConfigFactory::createScoringFunction(
    const Envoy::Protobuf::Message& message) {
  const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_rps::SigmoidScoringFunctionConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<SigmoidScoringFunction>(config);
}

REGISTER_FACTORY(SigmoidScoringFunctionConfigFactory, ScoringFunctionConfigFactory);

SigmoidScoringFunction::SigmoidScoringFunction(
    const nighthawk::adaptive_rps::SigmoidScoringFunctionConfig& config)
    : threshold_{config.threshold()}, k_{config.k()} {}

double SigmoidScoringFunction::EvaluateMetric(double value) const {
  return 1.0 - 2.0 / (1.0 + exp(-k_ * (value - threshold_)));
}

} // namespace AdaptiveRps
} // namespace Nighthawk
