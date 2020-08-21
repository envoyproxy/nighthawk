#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/client/options.pb.h"
#include "envoy/registry/registry.h"
#include "adaptive_load/plugin_loader.h"
#include "test/adaptive_load/fake_plugins/fake_input_variable_setter/fake_input_variable_setter.h"

#include "external/envoy/source/common/config/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::nighthawk::adaptive_load::BenchmarkResult;
using ::nighthawk::adaptive_load::FakeInputVariableSetterConfig;
using ::nighthawk::client::CommandLineOptions;
using ::testing::HasSubstr;

TEST(FakeInputVariableSetterConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.fake_input_variable_setter");
  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();
  FakeInputVariableSetterConfig expected_config;
  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(FakeInputVariableSetterConfigFactory, FactoryRegistersUnderCorrectName) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.fake_input_variable_setter");
  EXPECT_EQ(config_factory.name(), "nighthawk.fake_input_variable_setter");
}

TEST(FakeInputVariableSetterConfigFactory, CreateInputVariableSetterCreatesCorrectPluginType) {
  FakeInputVariableSetterConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  CommandLineOptions options;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.fake_input_variable_setter");
  InputVariableSetterPtr plugin = config_factory.createInputVariableSetter(config_any);
  EXPECT_NE(dynamic_cast<FakeInputVariableSetter*>(plugin.get()), nullptr);
}

TEST(FakeInputVariableSetterConfigFactory, ValidateConfigWithBadConfigProtoReturnsError) {
  Envoy::ProtobufWkt::Any empty_any;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.fake_input_variable_setter");
  absl::Status status = config_factory.ValidateConfig(empty_any);
  EXPECT_THAT(status.message(), HasSubstr("Failed to parse"));
}

TEST(FakeInputVariableSetterConfigFactory, ValidateConfigWithArtificialValidationErrorReturnsError) {
  FakeInputVariableSetterConfig config;
  const int kExpectedStatusCode = static_cast<int>(absl::StatusCode::kDataLoss);
  const std::string kExpectedStatusMessage = "artificial validation failure";
  config.mutable_artificial_validation_failure()->set_code(kExpectedStatusCode);
  config.mutable_artificial_validation_failure()->set_message(kExpectedStatusMessage);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.fake_input_variable_setter");
  absl::Status status = config_factory.ValidateConfig(config_any);
	EXPECT_EQ(static_cast<int>(status.code()), kExpectedStatusCode);
  EXPECT_EQ(status.message(), kExpectedStatusMessage);
}

TEST(FakeInputVariableSetterConfigFactory, ValidateConfigWithDefaultConfigReturnsOk) {
  FakeInputVariableSetterConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.fake_input_variable_setter");
  absl::Status status = config_factory.ValidateConfig(config_any);
  EXPECT_TRUE(status.ok());
}

TEST(FakeInputVariableSetterConfigFactory, ValidateConfigWithValidConfigReturnsOk) {
  FakeInputVariableSetterConfig config;
  config.set_adjustment_factor(1.0);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.fake_input_variable_setter");
  absl::Status status = config_factory.ValidateConfig(config_any);
  EXPECT_TRUE(status.ok());
}

TEST(MakeFakeInputVariableSetterConfig, ActivatesFakeInputVariableSetter) {
  absl::StatusOr<InputVariableSetterPtr> plugin_or = LoadInputVariableSetterPlugin(MakeFakeInputVariableSetterConfig(0));
  ASSERT_TRUE(plugin_or.ok());
  EXPECT_NE(dynamic_cast<FakeInputVariableSetter*>(plugin_or.value().get()), nullptr);
}

TEST(MakeFakeInputVariableSetterConfig, SetsInputWithDefaultConfigProtoValue) {
  absl::StatusOr<InputVariableSetterPtr> plugin_or = LoadInputVariableSetterPlugin(MakeFakeInputVariableSetterConfig(0));
  ASSERT_TRUE(plugin_or.ok());
  auto* plugin = dynamic_cast<FakeInputVariableSetter*>(plugin_or.value().get());
  ASSERT_NE(plugin, nullptr);
  CommandLineOptions options;
  absl::Status status = plugin->SetInputVariable(options, 123);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(options.connections().value(), 123);
}

TEST(MakeFakeInputVariableSetterConfig, SetsInputWithSpecifiedConfigProtoValue) {
  absl::StatusOr<InputVariableSetterPtr> plugin_or = LoadInputVariableSetterPlugin(MakeFakeInputVariableSetterConfig(100));
  ASSERT_TRUE(plugin_or.ok());
  auto* plugin = dynamic_cast<FakeInputVariableSetter*>(plugin_or.value().get());
  ASSERT_NE(plugin, nullptr);
  CommandLineOptions options;
  absl::Status status = plugin->SetInputVariable(options, 123);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(options.connections().value(), 12300);
}

} // namespace
} // namespace Nighthawk
