#include "test/adaptive_load/fake_plugins/fake_input_variable_setter/fake_input_variable_setter.h"

namespace Nighthawk {

namespace {

absl::Status StatusFromProtoRpcStatus(const google::rpc::Status& status_proto) {
  return absl::Status(static_cast<absl::StatusCode>(status_proto.code()), status_proto.message());
}

} // namespace

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

absl::Status
FakeInputVariableSetterConfigFactory::ValidateConfig(const Envoy::Protobuf::Message& message) const {
  try {
    const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
    nighthawk::adaptive_load::FakeInputVariableSetterConfig config;
    Envoy::MessageUtil::unpackTo(any, config);
    if (config.has_artificial_validation_failure()) {
      return StatusFromProtoRpcStatus(config.artificial_validation_failure());
    }
    return absl::OkStatus();
  } catch (const Envoy::EnvoyException& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse FakeInputVariableSetterConfig proto: ", e.what()));
  }
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
