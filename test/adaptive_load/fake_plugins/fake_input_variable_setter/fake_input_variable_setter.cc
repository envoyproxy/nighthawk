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
  if (input_value < 0) {
    return absl::InvalidArgumentError(
        "Artificial SetInputVariable failure triggered by negative value.");
  }
  command_line_options.mutable_connections()->set_value(
      static_cast<uint32_t>(input_value * adjustment_factor_));
  return absl::OkStatus();
}

std::string FakeInputVariableSetterConfigFactory::name() const {
  return "nighthawk.fake_input_variable_setter";
}

Envoy::ProtobufTypes::MessagePtr FakeInputVariableSetterConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_load::FakeInputVariableSetterConfig>();
}

InputVariableSetterPtr FakeInputVariableSetterConfigFactory::createInputVariableSetter(
    const Envoy::Protobuf::Message& message) {
  const auto& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_load::FakeInputVariableSetterConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<FakeInputVariableSetter>(config);
}

absl::Status FakeInputVariableSetterConfigFactory::ValidateConfig(
    const Envoy::Protobuf::Message& message) const {
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
MakeFakeInputVariableSetterConfig(uint32_t adjustment_factor) {
  envoy::config::core::v3::TypedExtensionConfig outer_config;
  outer_config.set_name("nighthawk.fake_input_variable_setter");
  nighthawk::adaptive_load::FakeInputVariableSetterConfig config;
  config.set_adjustment_factor(adjustment_factor);
  outer_config.mutable_typed_config()->PackFrom(config);
  return outer_config;
}

envoy::config::core::v3::TypedExtensionConfig MakeFakeInputVariableSetterConfigWithValidationError(
    const absl::Status& artificial_validation_error) {
  envoy::config::core::v3::TypedExtensionConfig outer_config;
  outer_config.set_name("nighthawk.fake_input_variable_setter");
  nighthawk::adaptive_load::FakeInputVariableSetterConfig config;
  config.mutable_artificial_validation_failure()->set_code(
      static_cast<int>(artificial_validation_error.code()));
  config.mutable_artificial_validation_failure()->set_message(
      std::string(artificial_validation_error.message()));
  outer_config.mutable_typed_config()->PackFrom(config);
  return outer_config;
}

} // namespace Nighthawk
