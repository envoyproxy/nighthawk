#include <typeinfo> // std::bad_cast

#include "adaptive_load/input_variable_setter_impl.h"
#include "adaptive_load/plugin_util.h"
#include "external/envoy/source/common/config/utility.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace AdaptiveLoad {

TEST(RequestsPerSecondInputVariableSetterFactoryTest, GeneratesEmptyConfigProto) {
  InputVariableSetterConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>("rps");

  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();
  
  nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig expected_config;

  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(RequestsPerSecondInputVariableSetterFactoryTest, CreatesRpsPlugin) {
  nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  Envoy::ProtobufWkt::Any any;
  any.PackFrom(config);

  InputVariableSetterConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>("rps");
  InputVariableSetterPtr rps_plugin = config_factory.createInputVariableSetter(any);

  EXPECT_NE(dynamic_cast<RequestsPerSecondInputVariableSetter*>(rps_plugin.get()), nullptr);
}

TEST(RequestsPerSecondInputVariableSetterTest, SetsCommandLineOptionsRpsValue) {
  nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  RequestsPerSecondInputVariableSetter setter(config);
  nighthawk::client::CommandLineOptions options;
  setter.SetInputVariable(&options, 5.0);
  EXPECT_EQ(options.requests_per_second().value(), 5);
}

} // namespace AdaptiveLoad
} // namespace Nighthawk