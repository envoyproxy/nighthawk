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

using ::Envoy::Http::TestResponseHeaderMapImpl;
using ::google::protobuf::TextFormat;
using ::nighthawk::LogResponseHeadersConfig;
using ::nighthawk::LogResponseHeadersOutput;
using ::testing::HasSubstr;

UserDefinedOutputPluginPtr CreatePlugin(const std::string& config_textproto, int worker_number) {
  LogResponseHeadersConfig config;
  TextFormat::ParseFromString(config_textproto, &config);

  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.log_response_headers_plugin");
  WorkerMetadata metadata;
  metadata.worker_number = worker_number;

  return factory.createUserDefinedOutputPlugin(config_any, metadata);
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
  UserDefinedOutputPluginPtr plugin = CreatePlugin("", /*worker_number=*/0);

  absl::StatusOr<Envoy::ProtobufWkt::Any> any_or = plugin->getPerWorkerOutput();
  EXPECT_TRUE(any_or.status().ok());
  EXPECT_TRUE(any_or->Is<LogResponseHeadersOutput>());
}

TEST(HandleResponseHeaders, LogsAllHeadersIfConfigured) {
  UserDefinedOutputPluginPtr plugin =
      CreatePlugin("log_all_headers:true logging_mode:LM_LOG_ALL_RESPONSES", /*worker_number=*/0);
  TestResponseHeaderMapImpl headers{};
  EXPECT_TRUE(plugin->handleResponseHeaders(headers).ok());
}

TEST(HandleResponseHeaders, LogsSpecifiedHeaders) {}

TEST(HandleResponseHeaders, OnlyLogsOnErrorsIfConfigured) {}

TEST(HandleResponseHeaders, FailsWithInvalidLoggingMode) {
  UserDefinedOutputPluginPtr plugin = CreatePlugin("", /*worker_number=*/0);
  TestResponseHeaderMapImpl headers{};
  EXPECT_EQ(plugin->handleResponseHeaders(headers).code(), absl::StatusCode::kInvalidArgument);
}

TEST(HandleResponseHeaders, FailsWithInvalidHeaderScope) {
  UserDefinedOutputPluginPtr plugin = CreatePlugin("log_all_headers:true", /*worker_number=*/0);
  TestResponseHeaderMapImpl headers{};
  EXPECT_EQ(plugin->handleResponseHeaders(headers).code(), absl::StatusCode::kInvalidArgument);
}

TEST(HandleResponseData, ReturnsOk) {
  UserDefinedOutputPluginPtr plugin = CreatePlugin("", /*worker_number=*/0);
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
