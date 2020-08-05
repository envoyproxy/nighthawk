#include "adaptive_load/scoring_function_impl.h"

#include <cmath>

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_load/scoring_function_impl.pb.h"

namespace Nighthawk {

std::string BinaryScoringFunctionConfigFactory::name() const { return "nighthawk.binary"; }

Envoy::ProtobufTypes::MessagePtr BinaryScoringFunctionConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_load::BinaryScoringFunctionConfig>();
}

ScoringFunctionPtr
BinaryScoringFunctionConfigFactory::createScoringFunction(const Envoy::Protobuf::Message& message) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_load::BinaryScoringFunctionConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<BinaryScoringFunction>(config);
}

REGISTER_FACTORY(BinaryScoringFunctionConfigFactory, ScoringFunctionConfigFactory);

BinaryScoringFunction::BinaryScoringFunction(
    const nighthawk::adaptive_load::BinaryScoringFunctionConfig& config)
    : upper_threshold_{config.has_upper_threshold() ? config.upper_threshold().value()
                                                    : std::numeric_limits<double>::infinity()},
      lower_threshold_{config.has_lower_threshold() ? config.lower_threshold().value()
                                                    : -std::numeric_limits<double>::infinity()} {}

double BinaryScoringFunction::EvaluateMetric(double value) const {
  return value <= upper_threshold_ && value >= lower_threshold_ ? 1.0 : -1.0;
}

std::string LinearScoringFunctionConfigFactory::name() const { return "nighthawk.linear"; }

Envoy::ProtobufTypes::MessagePtr LinearScoringFunctionConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_load::LinearScoringFunctionConfig>();
}

ScoringFunctionPtr
LinearScoringFunctionConfigFactory::createScoringFunction(const Envoy::Protobuf::Message& message) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_load::LinearScoringFunctionConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<LinearScoringFunction>(config);
}

REGISTER_FACTORY(LinearScoringFunctionConfigFactory, ScoringFunctionConfigFactory);

LinearScoringFunction::LinearScoringFunction(
    const nighthawk::adaptive_load::LinearScoringFunctionConfig& config)
    : threshold_{config.threshold()}, k_{config.k()} {}

double LinearScoringFunction::EvaluateMetric(double value) const {
  return k_ * (threshold_ - value);
}

} // namespace Nighthawk
