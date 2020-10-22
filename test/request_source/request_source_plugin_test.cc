#include "envoy/common/exception.h"

#include "external/envoy/source/common/config/utility.h"
#include "external/envoy/test/mocks/api/mocks.h"
#include "external/envoy/test/mocks/stats/mocks.h"
#include "external/envoy/test/test_common/file_system_for_test.h"
#include "external/envoy/test/test_common/utility.h"

#include "request_source/request_options_list_plugin_impl.h"

#include "test/request_source/stub_plugin_impl.h"
#include "test/test_common/environment.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {
using nighthawk::request_source::FileBasedOptionsListRequestSourceConfig;
using nighthawk::request_source::InLineOptionsListRequestSourceConfig;
using nighthawk::request_source::StubPluginConfig;
using ::testing::NiceMock;
using ::testing::Test;
nighthawk::request_source::FileBasedOptionsListRequestSourceConfig
MakeFileBasedPluginConfigWithTestYaml(absl::string_view request_file) {
  nighthawk::request_source::FileBasedOptionsListRequestSourceConfig config;
  config.mutable_file_path()->assign(request_file);
  config.mutable_max_file_size()->set_value(4000);
  return config;
}
nighthawk::request_source::InLineOptionsListRequestSourceConfig
MakeInLinePluginConfig(nighthawk::client::RequestOptionsList options_list, int num_requests) {
  nighthawk::request_source::InLineOptionsListRequestSourceConfig config;
  *config.mutable_options_list() = std::move(options_list);
  config.set_num_requests(num_requests);
  return config;
}

class StubRequestSourcePluginTest : public Test {
public:
  StubRequestSourcePluginTest() : api_(Envoy::Api::createApiForTest(stats_store_)) {}
  Envoy::Stats::MockIsolatedStatsStore stats_store_;
  Envoy::Api::ApiPtr api_;
};

class FileBasedRequestSourcePluginTest : public Test {
public:
  FileBasedRequestSourcePluginTest() : api_(Envoy::Api::createApiForTest(stats_store_)) {}
  Envoy::Stats::MockIsolatedStatsStore stats_store_;
  Envoy::Api::ApiPtr api_;
};

class InLineRequestSourcePluginTest : public Test {
public:
  InLineRequestSourcePluginTest() : api_(Envoy::Api::createApiForTest(stats_store_)) {}
  Envoy::Stats::MockIsolatedStatsStore stats_store_;
  Envoy::Api::ApiPtr api_;
};
TEST_F(StubRequestSourcePluginTest, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.stub-request-source-plugin");
  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::request_source::StubPluginConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(Envoy::MessageUtil()(*empty_config, expected_config));
}

TEST_F(StubRequestSourcePluginTest, FactoryRegistrationUsesCorrectPluginName) {
  nighthawk::request_source::StubPluginConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.stub-request-source-plugin");
  EXPECT_EQ(config_factory.name(), "nighthawk.stub-request-source-plugin");
}

TEST_F(StubRequestSourcePluginTest, CreateRequestSourcePluginCreatesCorrectPluginType) {
  nighthawk::request_source::StubPluginConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.stub-request-source-plugin");
  auto header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  EXPECT_NE(dynamic_cast<StubRequestSource*>(plugin.get()), nullptr);
}
TEST_F(StubRequestSourcePluginTest, CreateRequestSourcePluginCreatesWorkingPlugin) {
  nighthawk::request_source::StubPluginConfig config;
  double test_value = 2;
  config.mutable_test_value()->set_value(test_value);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.stub-request-source-plugin");
  auto template_header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(template_header));
  Nighthawk::RequestGenerator generator = plugin->get();
  Nighthawk::RequestPtr request = generator();
  Nighthawk::HeaderMapPtr header = request->header();
  ASSERT_EQ(header->get(Envoy::Http::LowerCaseString("test_value")).size(), 1);
  EXPECT_EQ(header->get(Envoy::Http::LowerCaseString("test_value"))[0]->value().getStringView(),
            absl::string_view(std::to_string(test_value)));
}
TEST_F(FileBasedRequestSourcePluginTest, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::request_source::FileBasedOptionsListRequestSourceConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(Envoy::MessageUtil()(*empty_config, expected_config));
}

TEST_F(FileBasedRequestSourcePluginTest, FactoryRegistrationUsesCorrectPluginName) {
  nighthawk::request_source::FileBasedOptionsListRequestSourceConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  EXPECT_EQ(config_factory.name(), "nighthawk.file-based-request-source-plugin");
}

TEST_F(FileBasedRequestSourcePluginTest, CreateRequestSourcePluginCreatesCorrectPluginType) {
  nighthawk::request_source::FileBasedOptionsListRequestSourceConfig config =
      MakeFileBasedPluginConfigWithTestYaml(
          TestEnvironment::runfilesPath("test/request_source/test_data/test-config.yaml"));
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  auto header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  EXPECT_NE(dynamic_cast<OptionsListRequestSource*>(plugin.get()), nullptr);
}

TEST_F(FileBasedRequestSourcePluginTest,
       CreateRequestSourcePluginGetsWorkingRequestGeneratorThatEndsAtNumRequest) {
  nighthawk::request_source::FileBasedOptionsListRequestSourceConfig config =
      MakeFileBasedPluginConfigWithTestYaml(
          TestEnvironment::runfilesPath("test/request_source/test_data/test-config.yaml"));
  config.set_num_requests(2);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  auto header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr file_based_request_source =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  Nighthawk::RequestGenerator generator = file_based_request_source->get();
  Nighthawk::RequestPtr request1 = generator();
  Nighthawk::RequestPtr request2 = generator();
  Nighthawk::RequestPtr request3 = generator();
  ASSERT_NE(request1, nullptr);
  ASSERT_NE(request2, nullptr);

  Nighthawk::HeaderMapPtr header1 = request1->header();
  Nighthawk::HeaderMapPtr header2 = request2->header();
  EXPECT_EQ(header1->getPathValue(), "/a");
  EXPECT_EQ(header2->getPathValue(), "/b");
  EXPECT_EQ(request3, nullptr);
}

TEST_F(FileBasedRequestSourcePluginTest,
       CreateRequestSourcePluginWithMoreNumRequestsThanInFileGetsRequestGeneratorThatLoops) {
  nighthawk::request_source::FileBasedOptionsListRequestSourceConfig config =
      MakeFileBasedPluginConfigWithTestYaml(
          TestEnvironment::runfilesPath("test/request_source/test_data/test-config.yaml"));
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  auto header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr file_based_request_source =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  Nighthawk::RequestGenerator generator = file_based_request_source->get();
  Nighthawk::RequestPtr request1 = generator();
  Nighthawk::RequestPtr request2 = generator();
  Nighthawk::RequestPtr request3 = generator();
  ASSERT_NE(request1, nullptr);
  ASSERT_NE(request2, nullptr);
  ASSERT_NE(request3, nullptr);

  Nighthawk::HeaderMapPtr header1 = request1->header();
  Nighthawk::HeaderMapPtr header2 = request2->header();
  Nighthawk::HeaderMapPtr header3 = request3->header();
  EXPECT_EQ(header1->getPathValue(), "/a");
  EXPECT_EQ(header2->getPathValue(), "/b");
  EXPECT_EQ(header3->getPathValue(), "/a");
}

TEST_F(InLineRequestSourcePluginTest, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.in-line-options-list-request-source-plugin");
  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::request_source::InLineOptionsListRequestSourceConfig expected_config;
  EXPECT_EQ(empty_config->DebugString(), expected_config.DebugString());
  EXPECT_TRUE(Envoy::MessageUtil()(*empty_config, expected_config));
}

TEST_F(InLineRequestSourcePluginTest, FactoryRegistrationUsesCorrectPluginName) {
  nighthawk::request_source::InLineOptionsListRequestSourceConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.in-line-options-list-request-source-plugin");
  EXPECT_EQ(config_factory.name(), "nighthawk.in-line-options-list-request-source-plugin");
}

TEST_F(InLineRequestSourcePluginTest, CreateRequestSourcePluginCreatesCorrectPluginType) {
  Envoy::MessageUtil util;
  nighthawk::client::RequestOptionsList options_list;
  util.loadFromFile(/*file to load*/ TestEnvironment::runfilesPath(
                        "test/request_source/test_data/test-config.yaml"),
                    /*out parameter*/ options_list,
                    /*validation visitor*/ Envoy::ProtobufMessage::getStrictValidationVisitor(),
                    /*Api*/ *api_,
                    /*use api boosting*/ true);
  nighthawk::request_source::InLineOptionsListRequestSourceConfig config =
      MakeInLinePluginConfig(options_list, /*num_requests*/ 2);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.in-line-options-list-request-source-plugin");
  auto header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  EXPECT_NE(dynamic_cast<OptionsListRequestSource*>(plugin.get()), nullptr);
}

TEST_F(InLineRequestSourcePluginTest,
       CreateRequestSourcePluginGetsWorkingRequestGeneratorThatEndsAtNumRequest) {
  Envoy::MessageUtil util;
  nighthawk::client::RequestOptionsList options_list;
  util.loadFromFile(/*file to load*/ TestEnvironment::runfilesPath(
                        "test/request_source/test_data/test-config.yaml"),
                    /*out parameter*/ options_list,
                    /*validation visitor*/ Envoy::ProtobufMessage::getStrictValidationVisitor(),
                    /*Api*/ *api_,
                    /*use api boosting*/ true);
  nighthawk::request_source::InLineOptionsListRequestSourceConfig config =
      MakeInLinePluginConfig(options_list, /*num_requests*/ 2);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.in-line-options-list-request-source-plugin");
  auto header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  Nighthawk::RequestGenerator generator = plugin->get();
  Nighthawk::RequestPtr request1 = generator();
  Nighthawk::RequestPtr request2 = generator();
  Nighthawk::RequestPtr request3 = generator();
  ASSERT_NE(request1, nullptr);
  ASSERT_NE(request2, nullptr);

  Nighthawk::HeaderMapPtr header1 = request1->header();
  Nighthawk::HeaderMapPtr header2 = request2->header();
  EXPECT_EQ(header1->getPathValue(), "/a");
  EXPECT_EQ(header2->getPathValue(), "/b");
  EXPECT_EQ(request3, nullptr);
}

TEST_F(InLineRequestSourcePluginTest,
       CreateRequestSourcePluginWithMoreNumRequestsThanInListGetsRequestGeneratorThatLoops) {
  Envoy::MessageUtil util;
  nighthawk::client::RequestOptionsList options_list;
  util.loadFromFile(/*file to load*/ TestEnvironment::runfilesPath(
                        "test/request_source/test_data/test-config.yaml"),
                    /*out parameter*/ options_list,
                    /*validation visitor*/ Envoy::ProtobufMessage::getStrictValidationVisitor(),
                    /*Api*/ *api_,
                    /*use api boosting*/ true);
  nighthawk::request_source::InLineOptionsListRequestSourceConfig config =
      MakeInLinePluginConfig(options_list, /*num_requests*/ 4);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.in-line-options-list-request-source-plugin");
  auto header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  Nighthawk::RequestGenerator generator = plugin->get();
  Nighthawk::RequestPtr request1 = generator();
  Nighthawk::RequestPtr request2 = generator();
  Nighthawk::RequestPtr request3 = generator();
  ASSERT_NE(request1, nullptr);
  ASSERT_NE(request2, nullptr);
  ASSERT_NE(request3, nullptr);

  Nighthawk::HeaderMapPtr header1 = request1->header();
  Nighthawk::HeaderMapPtr header2 = request2->header();
  Nighthawk::HeaderMapPtr header3 = request3->header();
  EXPECT_EQ(header1->getPathValue(), "/a");
  EXPECT_EQ(header2->getPathValue(), "/b");
  EXPECT_EQ(header3->getPathValue(), "/a");
}
} // namespace
} // namespace Nighthawk
