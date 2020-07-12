#include "adaptive_load/input_variable_setter_impl.h"

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

namespace Nighthawk {
namespace AdaptiveLoad {

RequestsPerSecondInputVariableSetter::RequestsPerSecondInputVariableSetter() {}
void RequestsPerSecondInputVariableSetter::SetInputVariable(
    nighthawk::client::CommandLineOptions* command_line_options, double input_value) {
  command_line_options->mutable_requests_per_second()->set_value(
      static_cast<unsigned int>(input_value));
}

std::string RequestsPerSecondInputVariableSetterConfigFactory::name() const { return "rps"; }

// Returns a dummy value since RequestsPerSecondInputVariableSetter doesn't have a config proto.
Envoy::ProtobufTypes::MessagePtr
RequestsPerSecondInputVariableSetterConfigFactory::createEmptyConfigProto() {
  return std::make_unique<Envoy::ProtobufWkt::Any>();
}

// Ignores |message| since RequestsPerSecondInputVariableSetter doesn't have a config proto.
InputVariableSetterPtr RequestsPerSecondInputVariableSetterConfigFactory::createInputVariableSetter(
    const Envoy::Protobuf::Message&) {
  return std::make_unique<RequestsPerSecondInputVariableSetter>();
}

REGISTER_FACTORY(RequestsPerSecondInputVariableSetterConfigFactory,
                 InputVariableSetterConfigFactory);

NumericHttpHeaderInputVariableSetter::NumericHttpHeaderInputVariableSetter(
    const nighthawk::adaptive_load::NumericHttpHeaderInputVariableSetterConfig& config)
    : header_name_{config.header_name()} {}

void NumericHttpHeaderInputVariableSetter::SetInputVariable(
    nighthawk::client::CommandLineOptions* command_line_options, double input_value) {
  envoy::config::core::v3::HeaderValueOption* header_value_option =
      command_line_options->mutable_request_options()->mutable_request_headers()->Add();
  header_value_option->mutable_append()->set_value(false);
  header_value_option->mutable_header()->set_key(header_name_);
  header_value_option->mutable_header()->set_value(absl::StrCat(static_cast<int>(input_value)));
}

std::string NumericHttpHeaderInputVariableSetterConfigFactory::name() const {
  return "http_header";
}
Envoy::ProtobufTypes::MessagePtr
NumericHttpHeaderInputVariableSetterConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_load::NumericHttpHeaderInputVariableSetterConfig>();
}

InputVariableSetterPtr NumericHttpHeaderInputVariableSetterConfigFactory::createInputVariableSetter(
    const Envoy::Protobuf::Message& message) {
  const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_load::NumericHttpHeaderInputVariableSetterConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<NumericHttpHeaderInputVariableSetter>(config);
}

REGISTER_FACTORY(NumericHttpHeaderInputVariableSetterConfigFactory,
                 InputVariableSetterConfigFactory);

} // namespace AdaptiveLoad
} // namespace Nighthawk