#include "envoy/registry/registry.h"

#include "external/envoy/source/common/config/utility.h"

#include "test/test_common/proto_matchers.h"

#include "test/user_defined_output/fake_plugin/fake_user_defined_output.h"
#include "test/user_defined_output/fake_plugin/fake_user_defined_output.pb.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::nighthawk::FakeUserDefinedOutput;
using ::nighthawk::FakeUserDefinedOutputConfig;
using ::testing::HasSubstr;

TEST(FakeUserDefinedOutputPluginFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
          "nighthawk.fake_user_defined_output");
  Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  FakeUserDefinedOutputConfig expected_config;
  EXPECT_THAT(*empty_config, EqualsProto(expected_config));
}

TEST(FakeUserDefinedOutputPluginFactory, FactoryRegistersUnderCorrectName) {
  FakeUserDefinedOutputConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
          "nighthawk.fake_user_defined_output");
  EXPECT_EQ(config_factory.name(), "nighthawk.fake_user_defined_output");
}

TEST(FakeUserDefinedOutputPluginFactory, CreateUserDefinedOutputPluginCreatesCorrectPluginType) {
  FakeUserDefinedOutputConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
          "nighthawk.fake_user_defined_output");
  UserDefinedOutputPluginPtr plugin = config_factory.createUserDefinedOutputPlugin(config_any, {});
  EXPECT_NE(dynamic_cast<FakeUserDefinedOutputPlugin*>(plugin.get()), nullptr);
}

} // namespace
} // namespace Nighthawk
