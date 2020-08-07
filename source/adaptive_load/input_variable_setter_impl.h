#pragma once

#include "envoy/registry/registry.h"

#include "nighthawk/adaptive_load/input_variable_setter.h"

#include "api/adaptive_load/input_variable_setter_impl.pb.h"

namespace Nighthawk {

/**
 * An InputVariableSetter that sets the |requests_per_second| field in the CommandLineOptions proto.
 */
class RequestsPerSecondInputVariableSetter : public InputVariableSetter {
public:
  /**
   * Constructs the class from an already valid config proto.
   *
   * @param config Valid plugin-specific config proto.
   */
  RequestsPerSecondInputVariableSetter(
      const nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig& config);
  absl::Status SetInputVariable(nighthawk::client::CommandLineOptions& command_line_options,
                                double input_value) override;
};

/**
 * A factory that creates an RequestsPerSecondInputVariableSetter from a
 * RequestsPerSecondInputVariableSetterConfig proto.
 */
class RequestsPerSecondInputVariableSetterConfigFactory : public InputVariableSetterConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  InputVariableSetterPtr
  createInputVariableSetter(const Envoy::Protobuf::Message& message) override;
};

// This factory is activated through LoadInputVariableSetterPlugin in plugin_util.h.
DECLARE_FACTORY(RequestsPerSecondInputVariableSetterConfigFactory);

} // namespace Nighthawk
