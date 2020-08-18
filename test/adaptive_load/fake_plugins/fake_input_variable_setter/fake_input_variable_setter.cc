#include "test/adaptive_load/fake_plugins/fake_input_variable_setter/fake_input_variable_setter.h"

namespace Nighthawk {

FakeInputVariableSetter::FakeInputVariableSetter(
    const nighthawk::adaptive_load::FakeInputVariableSetterConfig& config)
    : adjustment_factor_{config.adjustment_factor() > 0 ? config.adjustment_factor() : 1} {}

absl::Status FakeInputVariableSetter::SetInputVariable(
    nighthawk::client::CommandLineOptions& command_line_options, double input_value) {
  command_line_options.mutable_connections()->set_value(static_cast<unsigned int>(input_value * adjustment_factor_));
  return absl::OkStatus();
}

std::string FakeInputVariableSetterConfigFactory::name() const {
  return "nighthawk.fake-input-variable-setter";
}

Envoy::ProtobufTypes::MessagePtr
FakeInputVariableSetterConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_load::FakeInputVariableSetterConfig>();
}

InputVariableSetterPtr FakeInputVariableSetterConfigFactory::createInputVariableSetter(
    const Envoy::Protobuf::Message& message) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_load::FakeInputVariableSetterConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<FakeInputVariableSetter>(config);
}

REGISTER_FACTORY(FakeInputVariableSetterConfigFactory, InputVariableSetterConfigFactory);

envoy::config::core::v3::TypedExtensionConfig
MakeFakeInputVariableSetterConfig(int adjustment_factor) {
  envoy::config::core::v3::TypedExtensionConfig outer_config;
  outer_config.set_name("nighthawk.fake-input-variable-setter");
  nighthawk::adaptive_load::FakeInputVariableSetterConfig config;
  config.set_adjustment_factor(adjustment_factor);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  *outer_config.mutable_typed_config() = config_any;
  return outer_config;
}

} // namespace Nighthawk