#include "external/envoy/source/common/config/utility.h"

#include "adaptive_load/input_variable_setter_impl.h"
#include "adaptive_load/plugin_util.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace AdaptiveLoad {
namespace {

TEST(RequestsPerSecondInputVariableSetterConfigFactoryTest, GeneratesEmptyConfigProto) {
  InputVariableSetterConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>("rps");

  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();

  nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig expected_config;

  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(RequestsPerSecondInputVariableSetterConfigFactoryTest, CreatesPlugin) {
  nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);

  InputVariableSetterConfigFactory& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>("rps");
  InputVariableSetterPtr plugin = config_factory.createInputVariableSetter(config_any);

  EXPECT_NE(dynamic_cast<RequestsPerSecondInputVariableSetter*>(plugin.get()), nullptr);
}

TEST(RequestsPerSecondInputVariableSetterTest, SetsCommandLineOptionsRpsValue) {
  nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  RequestsPerSecondInputVariableSetter setter(config);
  nighthawk::client::CommandLineOptions options;
  setter.SetInputVariable(&options, 5.0);
  EXPECT_EQ(options.requests_per_second().value(), 5);
}

} // namespace
} // namespace AdaptiveLoad
} // namespace Nighthawk