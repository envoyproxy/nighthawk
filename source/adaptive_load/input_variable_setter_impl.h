#pragma once

#include "nighthawk/adaptive_load/input_variable_setter.h"

#include "api/adaptive_load/input_variable_setter_impl.pb.h"

namespace Nighthawk {
namespace AdaptiveLoad {

// An InputVariableSetter that sets the |requests_per_second| field in the CommandLineOptions proto.
class RequestsPerSecondInputVariableSetter : public InputVariableSetter {
public:
  RequestsPerSecondInputVariableSetter();
  void SetInputVariable(nighthawk::client::CommandLineOptions* command_line_options,
                        double input_value) override;
};

// A factory that creates a RequestsPerSecondInputVariableSetter. Ignores |message| because
// InputVariableSetter does not define a config proto.
class RequestsPerSecondInputVariableSetterConfigFactory : public InputVariableSetterConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  // Ignores |message| because RequestsPerSecondInputVariableSetter does not use a config proto.
  InputVariableSetterPtr createInputVariableSetter(const Envoy::Protobuf::Message&) override;
};

// An InputVariableSetter that sets an HTTP header in the CommandLineOptions proto to the string
// representation of the input variable.
class HttpHeaderInputVariableSetter : public InputVariableSetter {
public:
  HttpHeaderInputVariableSetter(
      const nighthawk::adaptive_load::HttpHeaderInputVariableSetterConfig& config);
  void SetInputVariable(nighthawk::client::CommandLineOptions* command_line_options,
                        double input_value) override;

private:
  std::string header_name_;
};

// A factory that creates an HttpHeaderInputVariableSetter from an
// HttpHeaderInputVariableSetterConfig proto.
class HttpHeaderInputVariableSetterConfigFactory : public InputVariableSetterConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  InputVariableSetterPtr
  createInputVariableSetter(const Envoy::Protobuf::Message& message) override;
};

} // namespace AdaptiveLoad
} // namespace Nighthawk