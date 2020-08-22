
#pragma once

#include "envoy/registry/registry.h"

#include "nighthawk/adaptive_load/input_variable_setter.h"

#include "api/client/options.pb.h"

#include "test/adaptive_load/fake_plugins/fake_input_variable_setter/fake_input_variable_setter.pb.h"

namespace Nighthawk {

/**
 * Non-default InputVariableSetter for testing.
 */
class FakeInputVariableSetter : public InputVariableSetter {
public:
  /**
   * Constructs the FakeInputVariableSetter from its custom config proto.
   *
   * @param config Custom config proto containing a value to be stored in |adjustment_factor_|.
   */
  FakeInputVariableSetter(const nighthawk::adaptive_load::FakeInputVariableSetterConfig& config);
  /**
   * Sets the value or simulates an error in setting it.
   *
   * @param input_value Value of |connections| to set in the CommandLineOptions proto. Pass a
   * negative number to simulate a failure to apply a value.
   *
   * @return Status OK if the value could be applied, an error if it could not be applied.
   */
  absl::Status SetInputVariable(nighthawk::client::CommandLineOptions& command_line_options,
                                double input_value) override;

private:
  // A multiplier defined in the config proto that adjusts the input value before applying it, in
  // order to test the propagation of both input and config.
  uint32_t adjustment_factor_;
};

/**
 * A factory that creates a FakeInputVariableSetter from a
 * FakeInputVariableSetterConfig proto.
 */
class FakeInputVariableSetterConfigFactory : public InputVariableSetterConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  InputVariableSetterPtr createInputVariableSetter(const Envoy::Protobuf::Message&) override;
  absl::Status ValidateConfig(const Envoy::Protobuf::Message& message) const override;
};

// This factory is activated through LoadInputVariableSetter in plugin_util.h.
DECLARE_FACTORY(FakeInputVariableSetterConfigFactory);

/**
 * Creates a valid TypedExtensionConfig proto that activates a FakeInputVariableSetter with a
 * FakeInputVariableSetterConfig.
 *
 * @param adjustment_factor A value for the config proto that the plugin should multiply the input
 * by before applying it, to test the propagation of both input and config.
 *
 * @return TypedExtensionConfig A proto that activates FakeInputVariableSetter by name and includes
 * a FakeInputVariableSetterConfig proto wrapped in an Any.
 */
envoy::config::core::v3::TypedExtensionConfig
MakeFakeInputVariableSetterConfig(uint32_t adjustment_factor);

/**
 * Creates a valid TypedExtensionConfig proto that activates a FakeInputVariableSetter with a
 * FakeInputVariableSetterConfig that fails validation.
 *
 * @param artificial_validation_error An error status.
 *
 * @return TypedExtensionConfig A proto that activates FakeInputVariableSetter by name and includes
 * a FakeInputVariableSetterConfig proto wrapped in an Any. This proto will fail validation when
 * attempting to load the plugin.
 */
envoy::config::core::v3::TypedExtensionConfig MakeFakeInputVariableSetterConfigWithValidationError(
    const absl::Status& artificial_validation_error);

} // namespace Nighthawk
