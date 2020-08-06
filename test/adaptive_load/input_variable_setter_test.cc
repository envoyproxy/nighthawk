#include "external/envoy/source/common/config/utility.h"

#include "adaptive_load/input_variable_setter_impl.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

TEST(RequestsPerSecondInputVariableSetterConfigFactoryTest, GeneratesEmptyConfigProto) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.rps");
  const Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();
  const nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig expected_config;
  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(Envoy::MessageUtil()(*message, expected_config));
}

TEST(RequestsPerSecondInputVariableSetterConfigFactoryTest, CreatesCorrectFactory) {
  const nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.rps");
  EXPECT_EQ(config_factory.name(), "nighthawk.rps");
}

TEST(RequestsPerSecondInputVariableSetterConfigFactoryTest, CreatesPlugin) {
  const nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.rps");
  InputVariableSetterPtr plugin = config_factory.createInputVariableSetter(config_any);
  EXPECT_NE(dynamic_cast<RequestsPerSecondInputVariableSetter*>(plugin.get()), nullptr);
}

TEST(RequestsPerSecondInputVariableSetterTest, SetsCommandLineOptionsRpsValue) {
  const nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  RequestsPerSecondInputVariableSetter setter(config);
  nighthawk::client::CommandLineOptions options;
  ASSERT_TRUE(setter.SetInputVariable(options, 5.0).ok());
  EXPECT_EQ(options.requests_per_second().value(), 5);
}

TEST(RequestsPerSecondInputVariableSetterTest, TruncatesNonIntegerRpsValue) {
  const nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  RequestsPerSecondInputVariableSetter setter(config);
  nighthawk::client::CommandLineOptions options;
  ASSERT_TRUE(setter.SetInputVariable(options, 5.3).ok());
  EXPECT_EQ(options.requests_per_second().value(), 5);
}

TEST(RequestsPerSecondInputVariableSetterTest, ReturnsErrorWithNegativeRpsValue) {
  const nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  RequestsPerSecondInputVariableSetter setter(config);
  nighthawk::client::CommandLineOptions options;
  EXPECT_THAT(setter.SetInputVariable(options, -100.0).message(),
              testing::HasSubstr("out of range"));
}

TEST(RequestsPerSecondInputVariableSetterTest, ReturnsErrorWithOversizedRpsValue) {
  const nighthawk::adaptive_load::RequestsPerSecondInputVariableSetterConfig config;
  RequestsPerSecondInputVariableSetter setter(config);
  nighthawk::client::CommandLineOptions options;
  EXPECT_THAT(setter.SetInputVariable(options, 1.0e100).message(),
              testing::HasSubstr("out of range"));
}

} // namespace

} // namespace Nighthawk
