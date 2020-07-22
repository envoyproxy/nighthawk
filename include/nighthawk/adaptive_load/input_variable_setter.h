#pragma once

#include "envoy/common/pure.h"
#include "envoy/config/core/v3/base.pb.h"
#include "envoy/config/typed_config.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/options.pb.h"

namespace Nighthawk {
namespace AdaptiveLoad {

// An interface for plugins that apply a StepController-computed input value to a CommandLineOptions
// proto. This may entail setting a numeric proto field directly, setting the value in a header, or
// otherwise manipulating the proto to reflect the number.
//
// See source/adaptive_load/input_variable_setter_impl.h for example plugins.
class InputVariableSetter {
public:
  virtual ~InputVariableSetter() = default;
  // Applies the numeric input value to the |CommandLineOptions| object.
  virtual void SetInputVariable(nighthawk::client::CommandLineOptions* command_line_options,
                                double input_value) PURE;
};

using InputVariableSetterPtr = std::unique_ptr<InputVariableSetter>;

// A factory that must be implemented for each InputVariableSetter plugin. It instantiates the
// specific InputVariableSetter class after unpacking the optional plugin-specific config proto.
class InputVariableSetterConfigFactory : public Envoy::Config::TypedFactory {
public:
  ~InputVariableSetterConfigFactory() override = default;
  std::string category() const override { return "nighthawk.input_variable_setter"; }
  // Instantiates the specific InputVariableSetter class. Casts |message| to Any, unpacks it to the
  // plugin-specific proto, and passes the strongly typed proto to the constructor.
  // If the plugin does not have a config proto, the constructor should not take an argument, and
  // createInputVariableSetter() should ignore |message|.
  virtual InputVariableSetterPtr
  createInputVariableSetter(const Envoy::Protobuf::Message& message) PURE;
};

} // namespace AdaptiveLoad
} // namespace Nighthawk
