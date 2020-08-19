#include "envoy/common/exception.h"
#include "common/request_source_plugin_impl.h"
#include "external/envoy/source/common/config/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {
using nighthawk::request_source::DummyPluginRequestSourceConfig;
TEST(DummyPluginRequestSourceConfigFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.dummy-request-source-plugin");
  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::request_source::DummyPluginRequestSourceConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(Envoy::MessageUtil()(*empty_config, expected_config));
}
TEST(DummyPluginRequestSourceConfigFactory, FactoryRegistrationUsesCorrectPluginName) {
  nighthawk::request_source::DummyPluginRequestSourceConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.dummy-request-source-plugin");
  EXPECT_EQ(config_factory.name(), "nighthawk.dummy-request-source-plugin");
}
TEST(DummyPluginRequestSourceConfigFactory, CreateRequestSourcePluginCreatesCorrectPluginType) {
  nighthawk::request_source::DummyPluginRequestSourceConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.dummy-request-source-plugin");
  RequestSourcePluginPtr plugin = config_factory.createRequestSourcePlugin(config_any);
  EXPECT_NE(dynamic_cast<DummyRequestSourcePlugin*>(plugin.get()), nullptr);
}
} // namespace
} // namespace Nighthawk
