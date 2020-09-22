#include "envoy/common/exception.h"

#include "external/envoy/source/common/config/utility.h"
#include "external/envoy/test/mocks/api/mocks.h"
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
using ::testing::NiceMock;
using ::testing::Test;

class DummyRequestSourcePluginTest : public Test {
public:
  DummyRequestSourcePluginTest()
      : context_(Envoy::Api::createApiForTest(stats_store_),
                 Envoy::Http::RequestHeaderMapImpl::create()) {}
  Nighthawk::RequestSourceContext context_;
  Envoy::Stats::MockIsolatedStatsStore stats_store_;
};

class FileBasedRequestSourcePluginTest : public Test {
public:
  FileBasedRequestSourcePluginTest()
      : context_(Envoy::Api::createApiForTest(stats_store_),
                 Envoy::Http::RequestHeaderMapImpl::create()) {}
  Nighthawk::RequestSourceContext context_;
  Envoy::Stats::MockIsolatedStatsStore stats_store_;
  nighthawk::request_source::FileBasedPluginRequestSourceConfig
  MakeFileBasedPluginConfigWithTestYaml(absl::string_view request_file) {
    nighthawk::request_source::FileBasedPluginRequestSourceConfig config;
    config.mutable_file_path()->assign(request_file);
    config.mutable_max_file_size()->set_value(4000);
    return config;
  }
};

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
  auto context = std::make_unique<RequestSourceContext>(context_);
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, std::move(context));
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
  auto context = std::make_unique<RequestSourceContext>(context_);
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, std::move(context));
  EXPECT_NE(dynamic_cast<FileBasedRequestSourcePlugin*>(plugin.get()), nullptr);
}

TEST_F(FileBasedRequestSourcePluginTest,
       CreateRequestSourcePluginGetsWorkingRequestGeneratorThatEndsAtNumRequest) {
  nighthawk::request_source::FileBasedPluginRequestSourceConfig config =
      MakeFileBasedPluginConfigWithTestYaml(
          TestEnvironment::runfilesPath("test/request_source/test_data/test-config.yaml"));
  config.mutable_num_requests()->set_value(2);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  auto context = std::make_unique<RequestSourceContext>(context_);

  RequestSourcePtr file_based_request_source =
      config_factory.createRequestSourcePlugin(config_any, std::move(context));
  auto generator = file_based_request_source->get();
  auto request = generator();
  auto request2 = generator();
  auto request3 = generator();
  auto header = request->header();
  auto header2 = request2->header();
  EXPECT_EQ(header->getPathValue(), "/a");
  EXPECT_EQ(header2->getPathValue(), "/b");
  EXPECT_EQ(request3, nullptr);
}

TEST_F(FileBasedRequestSourcePluginTest,
       CreateRequestSourcePluginWithMoreNumRequestsThanInFileGetsWorkingRequestGeneratorThatLoops) {
  nighthawk::request_source::FileBasedPluginRequestSourceConfig config =
      MakeFileBasedPluginConfigWithTestYaml(
          TestEnvironment::runfilesPath("test/request_source/test_data/test-config.yaml"));
  config.mutable_num_requests()->set_value(4);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  auto context = std::make_unique<RequestSourceContext>(context_);
  RequestSourcePtr file_based_request_source =
      config_factory.createRequestSourcePlugin(config_any, std::move(context));
  auto generator = file_based_request_source->get();
  auto request = generator();
  auto request2 = generator();
  auto request3 = generator();
  auto header = request->header();
  auto header2 = request2->header();
  auto header3 = request3->header();
  EXPECT_EQ(header->getPathValue(), "/a");
  EXPECT_EQ(header2->getPathValue(), "/b");
  EXPECT_EQ(header3->getPathValue(), "/a");
}
TEST_F(FileBasedRequestSourcePluginTest, CreateMultipleRequestSourcePluginReadsFileOnce) {
  nighthawk::request_source::FileBasedPluginRequestSourceConfig config =
      MakeFileBasedPluginConfigWithTestYaml(
          TestEnvironment::runfilesPath("test/request_source/test_data/test-config.yaml"));
  config.mutable_num_requests()->set_value(4);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  auto context = std::make_unique<RequestSourceContext>(
      Envoy::Api::createApiForTest(stats_store_), Envoy::Http::RequestHeaderMapImpl::create());
  auto context2 = std::make_unique<RequestSourceContext>(
      Envoy::Api::createApiForTest(stats_store_), Envoy::Http::RequestHeaderMapImpl::create());
  RequestSourcePtr file_based_request_source =
      config_factory.createRequestSourcePlugin(config_any, std::move(context));
  RequestSourcePtr file_based_request_source2 =
      config_factory.createRequestSourcePlugin(config_any, std::move(context2));
}
} // namespace
} // namespace Nighthawk
