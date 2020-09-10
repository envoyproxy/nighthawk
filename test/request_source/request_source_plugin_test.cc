#include "envoy/common/exception.h"

#include "external/envoy/source/common/config/utility.h"
#include "external/envoy/test/mocks/stats/mocks.h"
#include "external/envoy/test/test_common/file_system_for_test.h"
#include "external/envoy/test/test_common/utility.h"

#include "common/request_source_plugin_impl.h"

#include "test/test_common/environment.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {
using nighthawk::request_source::DummyPluginRequestSourceConfig;
using nighthawk::request_source::FileBasedPluginRequestSourceConfig;
using ::testing::Test;

class DummyRequestSourcePluginTest : public Test {
public:
  DummyRequestSourcePluginTest() : api_(Envoy::Api::createApiForTest(stats_store_)) {}
  Envoy::Api::ApiPtr api_;
  Envoy::Stats::MockIsolatedStatsStore stats_store_;
}; // RequestSourcePluginTest
class FileBasedRequestSourcePluginTest : public Test {
public:
  FileBasedRequestSourcePluginTest() : api_(Envoy::Api::createApiForTest(stats_store_)) {}
  Envoy::Api::ApiPtr api_;
  Envoy::Stats::MockIsolatedStatsStore stats_store_;
  nighthawk::request_source::FileBasedPluginRequestSourceConfig
  MakeFileBasedPluginConfigWithTestYaml(std::string request_file) {
    nighthawk::request_source::FileBasedPluginRequestSourceConfig config;
    config.mutable_uri()->assign("http://foo/");
    config.mutable_file_path()->assign(request_file);
    return config;
  }
}; // RequestSourcePluginTest

TEST_F(DummyRequestSourcePluginTest, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.dummy-request-source-plugin");
  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::request_source::DummyPluginRequestSourceConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(Envoy::MessageUtil()(*empty_config, expected_config));
}
TEST_F(DummyRequestSourcePluginTest, FactoryRegistrationUsesCorrectPluginName) {
  nighthawk::request_source::DummyPluginRequestSourceConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.dummy-request-source-plugin");
  EXPECT_EQ(config_factory.name(), "nighthawk.dummy-request-source-plugin");
}
TEST_F(DummyRequestSourcePluginTest, CreateRequestSourcePluginCreatesCorrectPluginType) {
  nighthawk::request_source::DummyPluginRequestSourceConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.dummy-request-source-plugin");
  RequestSourcePluginPtr plugin = config_factory.createRequestSourcePlugin(config_any, *api_);
  EXPECT_NE(dynamic_cast<DummyRequestSourcePlugin*>(plugin.get()), nullptr);
}
TEST_F(FileBasedRequestSourcePluginTest, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::request_source::FileBasedPluginRequestSourceConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(Envoy::MessageUtil()(*empty_config, expected_config));
}
TEST_F(FileBasedRequestSourcePluginTest, FactoryRegistrationUsesCorrectPluginName) {
  nighthawk::request_source::FileBasedPluginRequestSourceConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  EXPECT_EQ(config_factory.name(), "nighthawk.file-based-request-source-plugin");
}
TEST_F(FileBasedRequestSourcePluginTest, CreateRequestSourcePluginCreatesCorrectPluginType) {
  nighthawk::request_source::FileBasedPluginRequestSourceConfig config =
      MakeFileBasedPluginConfigWithTestYaml(
          TestEnvironment::runfilesPath("test/request_source/test_data/test-config.yaml"));
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  RequestSourcePluginPtr plugin = config_factory.createRequestSourcePlugin(config_any, *api_);
  EXPECT_NE(dynamic_cast<FileBasedRequestSourcePlugin*>(plugin.get()), nullptr);
}
TEST_F(FileBasedRequestSourcePluginTest, CreateRequestSourcePluginGetsWorkingRequestGenerator) {
  nighthawk::request_source::FileBasedPluginRequestSourceConfig config =
      MakeFileBasedPluginConfigWithTestYaml(
          TestEnvironment::runfilesPath("test/request_source/test_data/test-config.yaml"));
  FileBasedRequestSourcePlugin file_based_request_source(config, *api_);
  auto generator = file_based_request_source.get();
  auto request = generator();
  auto request2 = generator();
  auto header = request->header();
  auto header2 = request2->header();
  EXPECT_EQ(header->getPathValue(), "/a");
  EXPECT_EQ(header2->getPathValue(), "/b");
}
} // namespace
} // namespace Nighthawk
