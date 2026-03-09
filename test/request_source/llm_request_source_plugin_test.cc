#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "source/request_source/llm_request_source_plugin.pb.h"
#include "source/request_source/llm_request_source_plugin_impl.h"

#include "nighthawk/common/request.h"
#include "nighthawk/common/request_source.h"
#include "source/common/common/assert.h"
#include "source/common/json/json_loader.h"

#include "external/envoy/source/common/http/header_map_impl.h"
#include "external/envoy/source/common/protobuf/protobuf.h"
#include "external/envoy/source/common/protobuf/utility.h"
#include "external/envoy/test/mocks/api/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::Envoy::Protobuf::TextFormat;
using ::testing::NiceMock;

TEST(LlmRequestSourcePluginTest, TestLlmRequestSourcePlugin) {
  nighthawk::LlmRequestSourcePluginConfig config;
  TextFormat::ParseFromString(R"pb(
    model_name: "test_model"
    req_token_count: 100
    resp_max_tokens: 100
    options_list {
      options {
        request_headers { header { key: "test_header" value: "test_value" } }
      }
    }
  )pb",
                              &config);
  Envoy::Http::RequestHeaderMapPtr headers = Envoy::Http::RequestHeaderMapImpl::create();
  LlmRequestSourcePluginFactory factory;
  NiceMock<Envoy::Api::MockApi> mock_api;
  Envoy::Protobuf::Any config_wrapper;
  config_wrapper.PackFrom(config);
  Nighthawk::RequestSourcePtr llm_request_source =
      factory.createRequestSourcePlugin(config_wrapper, mock_api, std::move(headers));
  ASSERT_NE(llm_request_source, nullptr);

  Nighthawk::RequestGenerator request_generator = llm_request_source->get();
  std::unique_ptr<Nighthawk::Request> request = request_generator();

  Envoy::Json::ObjectSharedPtr json_object =
      Envoy::Json::Factory::loadFromString(request->body()).value();

  EXPECT_EQ(json_object->getString("model").value(), "test_model");
  EXPECT_EQ(json_object->getInteger("max_tokens").value(), 100);

  std::vector<Envoy::Json::ObjectSharedPtr> messages =
      json_object->getObjectArray("messages").value();
  Envoy::Json::ObjectSharedPtr first_message_obj = messages[0];
  std::string content = first_message_obj->getString("content").value();
  std::vector<absl::string_view> tokens = absl::StrSplit(content, ' ');
  EXPECT_EQ(tokens.size(), 100);
  EXPECT_EQ(request->header()->get(Envoy::Http::LowerCaseString("test_header")).size(), 1);
}

TEST(LlmRequestSourcePluginTest, TestLlmRequestSourcePluginFactory) {
  nighthawk::LlmRequestSourcePluginConfig config;
  TextFormat::ParseFromString(R"pb(
    model_name: "test_model"
    req_token_count: 100
    resp_max_tokens: 100
    options_list {
      options {
        request_headers { header { key: "test_header" value: "test_value" } }
      }
    }
  )pb",
                              &config);
  Envoy::Http::RequestHeaderMapPtr headers = Envoy::Http::RequestHeaderMapImpl::create();
  LlmRequestSourcePluginFactory factory;
  NiceMock<Envoy::Api::MockApi> mock_api;
  Envoy::Protobuf::Any config_wrapper;
  config_wrapper.PackFrom(config);
  Nighthawk::RequestSourcePtr llm_request_source =
      factory.createRequestSourcePlugin(config_wrapper, mock_api, std::move(headers));
  ASSERT_NE(llm_request_source, nullptr);
}

} // namespace
} // namespace Nighthawk
