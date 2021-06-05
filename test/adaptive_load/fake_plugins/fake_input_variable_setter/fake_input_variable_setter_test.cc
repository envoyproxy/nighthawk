#include "envoy/registry/registry.h"

#include "external/envoy/source/common/config/utility.h"

#include "api/adaptive_load/benchmark_result.pb.h"
#include "api/client/options.pb.h"

#include "source/adaptive_load/plugin_loader.h"

#include "test/adaptive_load/fake_plugins/fake_input_variable_setter/fake_input_variable_setter.h"

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

TEST(FakeInputVariableSetterConfigFactory,
     ValidateConfigWithArtificialValidationErrorReturnsError) {
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

TEST(FakeInputVariableSetter, AppliesInputVariableWithNonnegativeInputValue) {
  const int kExpectedConnectionsValue = 123;
  FakeInputVariableSetterConfig config;
  FakeInputVariableSetter plugin(config);
  CommandLineOptions options;
  absl::Status status =
      plugin.SetInputVariable(options, static_cast<double>(kExpectedConnectionsValue));
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(options.connections().value(), kExpectedConnectionsValue);
}

TEST(FakeInputVariableSetter, AppliesInputVariableWithAdjustmentFactor) {
  const int kExpectedConnectionsValue = 123;
  const int kAdjustmentFactor = 100;
  FakeInputVariableSetterConfig config;
  config.set_adjustment_factor(kAdjustmentFactor);
  FakeInputVariableSetter plugin(config);
  CommandLineOptions options;
  absl::Status status =
      plugin.SetInputVariable(options, static_cast<double>(kExpectedConnectionsValue));
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(options.connections().value(), kExpectedConnectionsValue * kAdjustmentFactor);
}

TEST(FakeInputVariableSetter, ReturnsErrorWithNegativeInputValue) {
  FakeInputVariableSetterConfig config;
  FakeInputVariableSetter plugin(config);
  CommandLineOptions options;
  absl::Status status = plugin.SetInputVariable(options, -1.0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.message(), "Artificial SetInputVariable failure triggered by negative value.");
}

TEST(MakeFakeInputVariableSetterConfig, ActivatesFakeInputVariableSetter) {
  absl::StatusOr<InputVariableSetterPtr> plugin_or =
      LoadInputVariableSetterPlugin(MakeFakeInputVariableSetterConfig(0));
  ASSERT_TRUE(plugin_or.ok());
  EXPECT_NE(dynamic_cast<FakeInputVariableSetter*>(plugin_or.value().get()), nullptr);
}

TEST(MakeFakeInputVariableSetterConfig, SetsInputWithSpecifiedConfigProtoValue) {
  const int kExpectedConnectionsValue = 123;
  const int kAdjustmentFactor = 100;
  absl::StatusOr<InputVariableSetterPtr> plugin_or =
      LoadInputVariableSetterPlugin(MakeFakeInputVariableSetterConfig(kAdjustmentFactor));
  ASSERT_TRUE(plugin_or.ok());
  auto* plugin = dynamic_cast<FakeInputVariableSetter*>(plugin_or.value().get());
  ASSERT_NE(plugin, nullptr);
  CommandLineOptions options;
  absl::Status status = plugin->SetInputVariable(options, kExpectedConnectionsValue);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(options.connections().value(), kExpectedConnectionsValue * kAdjustmentFactor);
}

TEST(MakeFakeInputVariableSetterConfigWithError, CreatesConfigProtoWithCorrectArtificalError) {
  std::string kValidationErrorMessage = "artificial validation error";
  absl::StatusOr<InputVariableSetterPtr> plugin_or =
      LoadInputVariableSetterPlugin(MakeFakeInputVariableSetterConfigWithValidationError(
          absl::DeadlineExceededError(kValidationErrorMessage)));
  EXPECT_EQ(plugin_or.status().code(), absl::StatusCode::kDeadlineExceeded);
  EXPECT_EQ(plugin_or.status().message(), kValidationErrorMessage);
}

} // namespace
} // namespace Nighthawk
