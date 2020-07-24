#include "adaptive_load/input_variable_setter_impl.h"

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

namespace Nighthawk {
namespace AdaptiveLoad {

RequestsPerSecondInputVariableSetter::RequestsPerSecondInputVariableSetter(
    const nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig&) {}

void RequestsPerSecondInputVariableSetter::SetInputVariable(
    nighthawk::client::CommandLineOptions& command_line_options, double input_value) {
  command_line_options.mutable_requests_per_second()->set_value(
      static_cast<unsigned int>(input_value));
}

std::string RequestsPerSecondInputVariableSetterConfigFactory::name() const {
  return "nighthawk.rps";
}

Envoy::ProtobufTypes::MessagePtr
RequestsPerSecondInputVariableSetterConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig>();
}

InputVariableSetterPtr RequestsPerSecondInputVariableSetterConfigFactory::createInputVariableSetter(
    const Envoy::Protobuf::Message& message) {
  const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<RequestsPerSecondInputVariableSetter>(config);
}

REGISTER_FACTORY(RequestsPerSecondInputVariableSetterConfigFactory,
                 InputVariableSetterConfigFactory);

} // namespace AdaptiveLoad
} // namespace Nighthawk