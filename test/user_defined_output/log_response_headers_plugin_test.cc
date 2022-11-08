#include <vector>

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/config/utility.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/protobuf.h"
#include "external/envoy/test/mocks/buffer/mocks.h"

#include "api/user_defined_output/log_response_headers.pb.h"

#include "source/user_defined_output/log_response_headers_plugin.h"

#include "test/test_common/proto_matchers.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::Envoy::Http::HeaderEntry;
using ::Envoy::Http::TestResponseHeaderMapImpl;
using ::google::protobuf::TextFormat;
using ::nighthawk::LogResponseHeadersConfig;
using ::nighthawk::LogResponseHeadersOutput;
using ::testing::HasSubstr;

// Fake Header Logger to enable testing of LogResponseHeadersPlugin. Keeps track of logged headers.
class FakeHeaderLogger : public HeaderLogger {
public:
  void LogHeader(const Envoy::Http::HeaderEntry& header_entry) override {
    header_entries_.push_back(&header_entry);
  }

  std::vector<const Envoy::Http::HeaderEntry*> getHeaderEntries() { return header_entries_; }

private:
  std::vector<const Envoy::Http::HeaderEntry*> header_entries_;
};

/**
 * @brief Create a Plugin object
 *
 * @param config_textproto the textproto of the LogResponseHeadersConfig
 * @param fake_logger if not nullptr, injects this header logger to use in the created
 * LogResponseHeadersPlugin.
 * @return UserDefinedOutputPluginPtr
 */
UserDefinedOutputPluginPtr CreatePlugin(const std::string& config_textproto,
                                        std::unique_ptr<HeaderLogger> header_logger) {
  LogResponseHeadersConfig config;
  TextFormat::ParseFromString(config_textproto, &config);

  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.log_response_headers_plugin");
  WorkerMetadata metadata;
  metadata.worker_number = 1;

  UserDefinedOutputPluginPtr plugin = factory.createUserDefinedOutputPlugin(config_any, metadata);
  if (header_logger != nullptr) {
    LogResponseHeadersPlugin* logging_plugin =
        dynamic_cast<LogResponseHeadersPlugin*>(plugin.get());
    logging_plugin->injectHeaderLogger(std::move(header_logger));
  }

  return plugin;
}

Envoy::ProtobufWkt::Any CreateOutput() {
  LogResponseHeadersOutput output;

  Envoy::ProtobufWkt::Any output_any;
  output_any.PackFrom(output);

  return output_any;
}

TEST(LogResponseHeadersPluginFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.log_response_headers_plugin");
  Envoy::ProtobufTypes::MessagePtr empty_config = factory.createEmptyConfigProto();
  LogResponseHeadersConfig expected_config;
  EXPECT_THAT(*empty_config, EqualsProto(expected_config));
}

TEST(LogResponseHeadersPluginFactory, FactoryRegistersUnderCorrectName) {
  LogResponseHeadersConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.log_response_headers_plugin");
  EXPECT_EQ(factory.name(), "nighthawk.log_response_headers_plugin");
}

TEST(LogResponseHeadersPluginFactory, CreateUserDefinedOutputPluginCreatesCorrectPluginType) {
  LogResponseHeadersConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.log_response_headers_plugin");
  UserDefinedOutputPluginPtr plugin = factory.createUserDefinedOutputPlugin(config_any, {});
  EXPECT_NE(dynamic_cast<LogResponseHeadersPlugin*>(plugin.get()), nullptr);
}

TEST(GetPerWorkerOutput, ReturnsProtoOfCorrectType) {
  std::unique_ptr<HeaderLogger> logger = nullptr;
  UserDefinedOutputPluginPtr plugin = CreatePlugin("", std::move(logger));

  absl::StatusOr<Envoy::ProtobufWkt::Any> any_or = plugin->getPerWorkerOutput();
  EXPECT_TRUE(any_or.status().ok());
  EXPECT_TRUE(any_or->Is<LogResponseHeadersOutput>());
}

TEST(HandleResponseHeaders, LogsAllHeadersIfConfigured) {
  std::unique_ptr<FakeHeaderLogger> logger = std::make_unique<FakeHeaderLogger>();
  FakeHeaderLogger* logger_ptr = logger.get();
  UserDefinedOutputPluginPtr plugin =
      CreatePlugin("log_all_headers:true logging_mode:LM_LOG_ALL_RESPONSES", std::move(logger));
  TestResponseHeaderMapImpl headers{
      {":status", "200"}, {"mytestheader1", "myvalue1"}, {"mytestheader2", "myvalue2"}};
  EXPECT_TRUE(plugin->handleResponseHeaders(headers).ok());
  std::vector<const HeaderEntry*> logged_headers = logger_ptr->getHeaderEntries();
  EXPECT_EQ(logged_headers.size(), 3);
  EXPECT_EQ(logged_headers[0]->key().getStringView(), ":status");
  EXPECT_EQ(logged_headers[0]->value().getStringView(), "200");
  EXPECT_EQ(logged_headers[1]->key().getStringView(), "mytestheader1");
  EXPECT_EQ(logged_headers[1]->value().getStringView(), "myvalue1");
  EXPECT_EQ(logged_headers[2]->key().getStringView(), "mytestheader2");
  EXPECT_EQ(logged_headers[2]->value().getStringView(), "myvalue2");
}

TEST(HandleResponseHeaders, LogsSpecifiedHeaders) {
  std::unique_ptr<FakeHeaderLogger> logger = std::make_unique<FakeHeaderLogger>();
  FakeHeaderLogger* logger_ptr = logger.get();
  UserDefinedOutputPluginPtr plugin = CreatePlugin(R"(logging_mode:LM_LOG_ALL_RESPONSES
      log_headers_with_names {
        header_names: "mytestheader1"
        header_names: "mytestheader2"
      })",
                                                   std::move(logger));
  TestResponseHeaderMapImpl headers{
      {":status", "200"}, {"mytestheader1", "myvalue1"}, {"mytestheader2", "myvalue2"}};
  EXPECT_TRUE(plugin->handleResponseHeaders(headers).ok());
  std::vector<const HeaderEntry*> logged_headers = logger_ptr->getHeaderEntries();
  EXPECT_EQ(logged_headers.size(), 2);
  EXPECT_EQ(logged_headers[0]->key().getStringView(), "mytestheader1");
  EXPECT_EQ(logged_headers[0]->value().getStringView(), "myvalue1");
  EXPECT_EQ(logged_headers[1]->key().getStringView(), "mytestheader2");
  EXPECT_EQ(logged_headers[1]->value().getStringView(), "myvalue2");
}

TEST(HandleResponseHeaders, OnlyLogsOnErrorsIfConfigured) {
  std::unique_ptr<FakeHeaderLogger> logger = std::make_unique<FakeHeaderLogger>();
  FakeHeaderLogger* logger_ptr = logger.get();
  UserDefinedOutputPluginPtr plugin =
      CreatePlugin("log_all_headers:true logging_mode:LM_ONLY_LOG_ERRORS", std::move(logger));
  TestResponseHeaderMapImpl headers_200{{":status", "200"}, {"mytestheader1", "myvalue1"}};
  TestResponseHeaderMapImpl headers_400{{":status", "400"}, {"mytestheader1", "myvalue1"}};
  TestResponseHeaderMapImpl headers_500{{":status", "500"}, {"mytestheader1", "myvalue1"}};
  EXPECT_TRUE(plugin->handleResponseHeaders(headers_200).ok());
  std::vector<const HeaderEntry*> logged_headers = logger_ptr->getHeaderEntries();
  EXPECT_TRUE(logged_headers.empty());

  EXPECT_TRUE(plugin->handleResponseHeaders(headers_400).ok());
  logged_headers = logger_ptr->getHeaderEntries();
  EXPECT_EQ(logged_headers.size(), 2);
  EXPECT_EQ(logged_headers[0]->key().getStringView(), ":status");
  EXPECT_EQ(logged_headers[0]->value().getStringView(), "400");
  EXPECT_EQ(logged_headers[1]->key().getStringView(), "mytestheader1");
  EXPECT_EQ(logged_headers[1]->value().getStringView(), "myvalue1");

  EXPECT_TRUE(plugin->handleResponseHeaders(headers_500).ok());
  logged_headers = logger_ptr->getHeaderEntries();
  EXPECT_EQ(logged_headers.size(), 4);
  EXPECT_EQ(logged_headers[2]->key().getStringView(), ":status");
  EXPECT_EQ(logged_headers[2]->value().getStringView(), "500");
  EXPECT_EQ(logged_headers[3]->key().getStringView(), "mytestheader1");
  EXPECT_EQ(logged_headers[3]->value().getStringView(), "myvalue1");
}

TEST(HandleResponseHeaders, FailsWithInvalidLoggingMode) {
  std::unique_ptr<HeaderLogger> logger = nullptr;
  UserDefinedOutputPluginPtr plugin = CreatePlugin("", std::move(logger));
  TestResponseHeaderMapImpl headers{};
  EXPECT_EQ(plugin->handleResponseHeaders(headers).code(), absl::StatusCode::kInvalidArgument);
}

TEST(HandleResponseHeaders, FailsWithInvalidHeaderScope) {
  std::unique_ptr<HeaderLogger> logger = nullptr;
  UserDefinedOutputPluginPtr plugin = CreatePlugin("log_all_headers:true", std::move(logger));
  TestResponseHeaderMapImpl headers{};
  EXPECT_EQ(plugin->handleResponseHeaders(headers).code(), absl::StatusCode::kInvalidArgument);
}

TEST(HandleResponseData, ReturnsOk) {
  std::unique_ptr<HeaderLogger> logger = nullptr;
  UserDefinedOutputPluginPtr plugin = CreatePlugin("", std::move(logger));
  Envoy::MockBuffer buffer;
  EXPECT_TRUE(plugin->handleResponseData(buffer).ok());
  EXPECT_TRUE(plugin->handleResponseData(buffer).ok());
}

TEST(AggregateGlobalOutput, ReturnsEmptyProto) {
  std::vector<Envoy::ProtobufWkt::Any> per_worker_outputs({
      CreateOutput(),
      CreateOutput(),
  });

  Envoy::ProtobufWkt::Any expected_aggregate = CreateOutput();

  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.log_response_headers_plugin");
  absl::StatusOr<Envoy::ProtobufWkt::Any> any_or =
      factory.AggregateGlobalOutput(per_worker_outputs);

  EXPECT_TRUE(any_or.status().ok());
  EXPECT_THAT(*any_or, EqualsProto(expected_aggregate));
}

} // namespace
} // namespace Nighthawk
