#include "adaptive_load/input_variable_setter_impl.h"

#include <limits>

#include "external/envoy/source/common/protobuf/protobuf.h"

namespace Nighthawk {

RequestsPerSecondInputVariableSetter::RequestsPerSecondInputVariableSetter(
    const nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig&) {}

absl::Status RequestsPerSecondInputVariableSetter::SetInputVariable(
    nighthawk::client::CommandLineOptions& command_line_options, double input_value) {
  if (input_value < 0.0 || input_value > std::numeric_limits<uint32_t>::max()) {
    return absl::InternalError(
        absl::StrCat("Input value out of range for uint32 requests_per_second: ", input_value));
  }
  command_line_options.mutable_requests_per_second()->set_value(
      static_cast<unsigned int>(input_value));
  return absl::OkStatus();
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
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<RequestsPerSecondInputVariableSetter>(config);
}

REGISTER_FACTORY(RequestsPerSecondInputVariableSetterConfigFactory,
                 InputVariableSetterConfigFactory);

} // namespace Nighthawk
