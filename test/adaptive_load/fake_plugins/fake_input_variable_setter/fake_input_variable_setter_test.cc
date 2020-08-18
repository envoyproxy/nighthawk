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

TEST(FakeInputVariableSetterConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.fake-input-variable-setter");
  Envoy::ProtobufTypes::MessagePtr message = config_factory.createEmptyConfigProto();
  FakeInputVariableSetterConfig expected_config;
  EXPECT_EQ(message->DebugString(), expected_config.DebugString());
}

TEST(FakeInputVariableSetterConfigFactory, FactoryRegistersUnderCorrectName) {
  FakeInputVariableSetterConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  CommandLineOptions options;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.fake-input-variable-setter");
  EXPECT_EQ(config_factory.name(), "nighthawk.fake-input-variable-setter");
}

TEST(FakeInputVariableSetterConfigFactory, FactoryCreatesCorrectPluginType) {
  FakeInputVariableSetterConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  CommandLineOptions options;
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<InputVariableSetterConfigFactory>(
          "nighthawk.fake-input-variable-setter");
  InputVariableSetterPtr plugin = config_factory.createInputVariableSetter(config_any);
  EXPECT_NE(dynamic_cast<FakeInputVariableSetter*>(plugin.get()), nullptr);
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