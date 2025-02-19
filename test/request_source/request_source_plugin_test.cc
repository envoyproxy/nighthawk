#include <cstdint>
#include <string>
#include <utility>

#include "envoy/api/api.h"
#include "envoy/common/exception.h"
#include "envoy/http/header_map.h"

#include "nighthawk/common/exception.h"
#include "nighthawk/common/request.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/request_source/request_source_plugin_config_factory.h"

#include "external/envoy/source/common/config/utility.h"
#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/protobuf.h"
#include "external/envoy/test/mocks/api/mocks.h"
#include "external/envoy/test/mocks/stats/mocks.h"
#include "external/envoy/test/test_common/file_system_for_test.h"
#include "external/envoy/test/test_common/utility.h"

#include "source/request_source/request_options_list_plugin_impl.h"

#include "test/request_source/stub_plugin_impl.h"
#include "test/test_common/environment.h"
#include "test/test_common/proto_matchers.h"

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
MakeFileBasedPluginConfigWithTestYaml(const std::string& request_file) {
  nighthawk::request_source::FileBasedOptionsListRequestSourceConfig config;
  config.set_file_path(request_file);
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
  EXPECT_THAT(*empty_config, EqualsProto(expected_config));
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
  Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  plugin->initOnThread();
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
  Envoy::Http::RequestHeaderMapPtr template_header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(template_header));
  plugin->initOnThread();
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
  EXPECT_THAT(*empty_config, EqualsProto(expected_config));
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
      MakeFileBasedPluginConfigWithTestYaml(Nighthawk::TestEnvironment::runfilesPath(
          "test/request_source/test_data/test-config-ab.yaml"));
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  plugin->initOnThread();
  EXPECT_NE(dynamic_cast<OptionsListRequestSource*>(plugin.get()), nullptr);
}

TEST_F(FileBasedRequestSourcePluginTest,
       CreateRequestSourcePluginGetsWorkingRequestGeneratorThatEndsAtNumRequest) {
  nighthawk::request_source::FileBasedOptionsListRequestSourceConfig config =
      MakeFileBasedPluginConfigWithTestYaml(Nighthawk::TestEnvironment::runfilesPath(
          "test/request_source/test_data/test-jsonconfig-ab.yaml"));
  config.set_num_requests(2);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr file_based_request_source =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  file_based_request_source->initOnThread();
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
  std::string body1 = request1->body();
  std::string body2 = request2->body();
  EXPECT_EQ(body1, R"({"message": "hello1"})");
  EXPECT_EQ(body2, R"({"message": "hello2"})");
  EXPECT_EQ(request3, nullptr);
}

TEST_F(FileBasedRequestSourcePluginTest, CreateRequestSourcePluginWithTooLargeAFileThrowsAnError) {
  nighthawk::request_source::FileBasedOptionsListRequestSourceConfig config =
      MakeFileBasedPluginConfigWithTestYaml(Nighthawk::TestEnvironment::runfilesPath(
          "test/request_source/test_data/test-config-ab.yaml"));
  const uint32_t max_file_size = 10;
  config.set_num_requests(2);
  config.mutable_max_file_size()->set_value(max_file_size);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
  EXPECT_THROW_WITH_REGEX(
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header)),
      NighthawkException, "file size must be less than max_file_size");
}

TEST_F(FileBasedRequestSourcePluginTest,
       CreateRequestSourcePluginWithMoreNumRequestsThanInFileGetsRequestGeneratorThatLoops) {
  nighthawk::request_source::FileBasedOptionsListRequestSourceConfig config =
      MakeFileBasedPluginConfigWithTestYaml(Nighthawk::TestEnvironment::runfilesPath(
          "test/request_source/test_data/test-config-ab.yaml"));
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr file_based_request_source =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  file_based_request_source->initOnThread();
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

TEST_F(
    FileBasedRequestSourcePluginTest,
    CreateRequestSourcePluginMultipleTimesWithDifferentConfigsCreatesDifferentWorkingRequestsSources) {
  nighthawk::request_source::FileBasedOptionsListRequestSourceConfig config_ab =
      MakeFileBasedPluginConfigWithTestYaml(Nighthawk::TestEnvironment::runfilesPath(
          "test/request_source/test_data/test-config-ab.yaml"));
  nighthawk::request_source::FileBasedOptionsListRequestSourceConfig config_c =
      MakeFileBasedPluginConfigWithTestYaml(Nighthawk::TestEnvironment::runfilesPath(
          "test/request_source/test_data/test-config-c.yaml"));
  config_ab.set_num_requests(2);
  config_c.set_num_requests(2);

  Envoy::ProtobufWkt::Any config_ab_any;
  config_ab_any.PackFrom(config_ab);
  Envoy::ProtobufWkt::Any config_c_any;
  config_c_any.PackFrom(config_c);

  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.file-based-request-source-plugin");
  Envoy::Http::RequestHeaderMapPtr header_ab = Envoy::Http::RequestHeaderMapImpl::create();
  Envoy::Http::RequestHeaderMapPtr header_c = Envoy::Http::RequestHeaderMapImpl::create();

  RequestSourcePtr plugin_ab =
      config_factory.createRequestSourcePlugin(config_ab_any, *api_, std::move(header_ab));
  RequestSourcePtr plugin_c =
      config_factory.createRequestSourcePlugin(config_c_any, *api_, std::move(header_c));

  plugin_ab->initOnThread();
  plugin_c->initOnThread();
  Nighthawk::RequestGenerator generator_ab = plugin_ab->get();
  Nighthawk::RequestPtr request_ab_1 = generator_ab();
  Nighthawk::RequestPtr request_ab_2 = generator_ab();
  Nighthawk::RequestPtr request_ab_3 = generator_ab();
  ASSERT_NE(request_ab_1, nullptr);
  ASSERT_NE(request_ab_2, nullptr);

  Nighthawk::HeaderMapPtr header_ab_1 = request_ab_1->header();
  Nighthawk::HeaderMapPtr header_ab_2 = request_ab_2->header();
  EXPECT_EQ(header_ab_1->getPathValue(), "/a");
  EXPECT_EQ(header_ab_2->getPathValue(), "/b");
  EXPECT_EQ(request_ab_3, nullptr);

  Nighthawk::RequestGenerator generator_c = plugin_c->get();
  Nighthawk::RequestPtr request_c_1 = generator_c();
  Nighthawk::RequestPtr request_c_2 = generator_c();
  Nighthawk::RequestPtr request_c_3 = generator_c();
  ASSERT_NE(request_c_1, nullptr);
  ASSERT_NE(request_c_2, nullptr);

  Nighthawk::HeaderMapPtr header_c_1 = request_c_1->header();
  Nighthawk::HeaderMapPtr header_c_2 = request_c_2->header();
  EXPECT_EQ(header_c_1->getPathValue(), "/c");
  EXPECT_EQ(header_c_2->getPathValue(), "/c");
  EXPECT_EQ(request_c_3, nullptr);
}

TEST_F(InLineRequestSourcePluginTest, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.in-line-options-list-request-source-plugin");
  const Envoy::ProtobufTypes::MessagePtr empty_config = config_factory.createEmptyConfigProto();
  const nighthawk::request_source::InLineOptionsListRequestSourceConfig expected_config;
  EXPECT_THAT(*empty_config, EqualsProto(expected_config));
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
  THROW_IF_NOT_OK(
      util.loadFromFile(/*file to load*/ Nighthawk::TestEnvironment::runfilesPath(
                            "test/request_source/test_data/test-config-ab.yaml"),
                        /*out parameter*/ options_list,
                        /*validation visitor*/ Envoy::ProtobufMessage::getStrictValidationVisitor(),
                        /*Api*/ *api_));
  nighthawk::request_source::InLineOptionsListRequestSourceConfig config =
      MakeInLinePluginConfig(options_list, /*num_requests*/ 2);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.in-line-options-list-request-source-plugin");
  Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  plugin->initOnThread();
  EXPECT_NE(dynamic_cast<OptionsListRequestSource*>(plugin.get()), nullptr);
}

TEST_F(InLineRequestSourcePluginTest,
       CreateRequestSourcePluginGetsWorkingRequestGeneratorThatEndsAtNumRequest) {
  Envoy::MessageUtil util;
  nighthawk::client::RequestOptionsList options_list;
  THROW_IF_NOT_OK(
      util.loadFromFile(/*file to load*/ Nighthawk::TestEnvironment::runfilesPath(
                            "test/request_source/test_data/test-jsonconfig-ab.yaml"),
                        /*out parameter*/ options_list,
                        /*validation visitor*/ Envoy::ProtobufMessage::getStrictValidationVisitor(),
                        /*Api*/ *api_));
  nighthawk::request_source::InLineOptionsListRequestSourceConfig config =
      MakeInLinePluginConfig(options_list, /*num_requests*/ 2);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.in-line-options-list-request-source-plugin");
  Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  plugin->initOnThread();
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
  std::string body1 = request1->body();
  std::string body2 = request2->body();
  EXPECT_EQ(body1, R"({"message": "hello1"})");
  EXPECT_EQ(body2, R"({"message": "hello2"})");
  EXPECT_EQ(request3, nullptr);
}

TEST_F(InLineRequestSourcePluginTest,
       CreateRequestSourcePluginWithMoreNumRequestsThanInListGetsRequestGeneratorThatLoops) {
  Envoy::MessageUtil util;
  nighthawk::client::RequestOptionsList options_list;
  THROW_IF_NOT_OK(
      util.loadFromFile(/*file to load*/ Nighthawk::TestEnvironment::runfilesPath(
                            "test/request_source/test_data/test-config-ab.yaml"),
                        /*out parameter*/ options_list,
                        /*validation visitor*/ Envoy::ProtobufMessage::getStrictValidationVisitor(),
                        /*Api*/ *api_));
  nighthawk::request_source::InLineOptionsListRequestSourceConfig config =
      MakeInLinePluginConfig(options_list, /*num_requests*/ 4);
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.in-line-options-list-request-source-plugin");
  Envoy::Http::RequestHeaderMapPtr header = Envoy::Http::RequestHeaderMapImpl::create();
  RequestSourcePtr plugin =
      config_factory.createRequestSourcePlugin(config_any, *api_, std::move(header));
  plugin->initOnThread();
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

TEST_F(
    InLineRequestSourcePluginTest,
    CreateRequestSourcePluginMultipleTimesWithDifferentConfigsCreatesDifferentWorkingRequestsSources) {
  Envoy::MessageUtil util;
  nighthawk::client::RequestOptionsList options_list_ab;
  THROW_IF_NOT_OK(
      util.loadFromFile(/*file to load*/ Nighthawk::TestEnvironment::runfilesPath(
                            "test/request_source/test_data/test-config-ab.yaml"),
                        /*out parameter*/ options_list_ab,
                        /*validation visitor*/ Envoy::ProtobufMessage::getStrictValidationVisitor(),
                        /*Api*/ *api_));
  nighthawk::client::RequestOptionsList options_list_c;
  THROW_IF_NOT_OK(
      util.loadFromFile(/*file to load*/ Nighthawk::TestEnvironment::runfilesPath(
                            "test/request_source/test_data/test-config-c.yaml"),
                        /*out parameter*/ options_list_c,
                        /*validation visitor*/ Envoy::ProtobufMessage::getStrictValidationVisitor(),
                        /*Api*/ *api_));

  nighthawk::request_source::InLineOptionsListRequestSourceConfig config_ab =
      MakeInLinePluginConfig(options_list_ab, /*num_requests*/ 2);
  nighthawk::request_source::InLineOptionsListRequestSourceConfig config_c =
      MakeInLinePluginConfig(options_list_c, /*num_requests*/ 2);

  Envoy::ProtobufWkt::Any config_ab_any;
  config_ab_any.PackFrom(config_ab);
  Envoy::ProtobufWkt::Any config_c_any;
  config_c_any.PackFrom(config_c);

  auto& config_factory =
      Envoy::Config::Utility::getAndCheckFactoryByName<RequestSourcePluginConfigFactory>(
          "nighthawk.in-line-options-list-request-source-plugin");
  Envoy::Http::RequestHeaderMapPtr header_ab = Envoy::Http::RequestHeaderMapImpl::create();
  Envoy::Http::RequestHeaderMapPtr header_c = Envoy::Http::RequestHeaderMapImpl::create();

  RequestSourcePtr plugin_ab =
      config_factory.createRequestSourcePlugin(config_ab_any, *api_, std::move(header_ab));
  RequestSourcePtr plugin_c =
      config_factory.createRequestSourcePlugin(config_c_any, *api_, std::move(header_c));

  plugin_ab->initOnThread();
  plugin_c->initOnThread();
  Nighthawk::RequestGenerator generator_ab = plugin_ab->get();
  Nighthawk::RequestPtr request_ab_1 = generator_ab();
  Nighthawk::RequestPtr request_ab_2 = generator_ab();
  Nighthawk::RequestPtr request_ab_3 = generator_ab();
  ASSERT_NE(request_ab_1, nullptr);
  ASSERT_NE(request_ab_2, nullptr);

  Nighthawk::HeaderMapPtr header_ab_1 = request_ab_1->header();
  Nighthawk::HeaderMapPtr header_ab_2 = request_ab_2->header();
  EXPECT_EQ(header_ab_1->getPathValue(), "/a");
  EXPECT_EQ(header_ab_2->getPathValue(), "/b");
  EXPECT_EQ(request_ab_3, nullptr);

  Nighthawk::RequestGenerator generator_c = plugin_c->get();
  Nighthawk::RequestPtr request_c_1 = generator_c();
  Nighthawk::RequestPtr request_c_2 = generator_c();
  Nighthawk::RequestPtr request_c_3 = generator_c();
  ASSERT_NE(request_c_1, nullptr);
  ASSERT_NE(request_c_2, nullptr);

  Nighthawk::HeaderMapPtr header_c_1 = request_c_1->header();
  Nighthawk::HeaderMapPtr header_c_2 = request_c_2->header();
  EXPECT_EQ(header_c_1->getPathValue(), "/c");
  EXPECT_EQ(header_c_2->getPathValue(), "/c");
  EXPECT_EQ(request_c_3, nullptr);
}

} // namespace
} // namespace Nighthawk
