#include "envoy/registry/registry.h"

#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/source/common/config/utility.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/protobuf.h"
#include "external/envoy/test/mocks/buffer/mocks.h"

#include "api/client/output.pb.h"

#include "test/test_common/proto_matchers.h"
#include "test/user_defined_output/fake_plugin/fake_user_defined_output.h"
#include "test/user_defined_output/fake_plugin/fake_user_defined_output.pb.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::Envoy::Http::TestResponseHeaderMapImpl;
using ::nighthawk::FakeUserDefinedOutput;
using ::nighthawk::FakeUserDefinedOutputConfig;
using ::proto2::TextFormat;
using ::testing::HasSubstr;

absl::StatusOr<UserDefinedOutputPluginPtr> CreatePlugin(const std::string& config_textproto,
                                                        int worker_number) {
  FakeUserDefinedOutputConfig config;
  TextFormat::ParseFromString(config_textproto, &config);

  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.fake_user_defined_output");
  WorkerMetadata metadata;
  metadata.worker_number = worker_number;

  return factory.createUserDefinedOutputPlugin(config_any, metadata);
}

// Packs a FakeUserDefinedOutput into an Any.
Envoy::ProtobufWkt::Any CreateOutputAny(const std::string& textproto) {
  FakeUserDefinedOutput output;
  TextFormat::ParseFromString(textproto, &output);

  Envoy::ProtobufWkt::Any output_any;
  output_any.PackFrom(output);

  return output_any;
}

// Packs a FakeUserDefinedOutput into a UserDefinedOutput
nighthawk::client::UserDefinedOutput CreateUserDefinedOutput(const std::string& textproto) {
  nighthawk::client::UserDefinedOutput output;
  *output.mutable_plugin_name() = "nighthawk.fake_user_defined_output";
  *output.mutable_typed_output() = CreateOutputAny(textproto);
  return output;
}

TEST(FakeUserDefinedOutputPluginFactory, CreateEmptyConfigProtoCreatesCorrectType) {
  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.fake_user_defined_output");
  Envoy::ProtobufTypes::MessagePtr empty_config = factory.createEmptyConfigProto();
  FakeUserDefinedOutputConfig expected_config;
  EXPECT_THAT(*empty_config, EqualsProto(expected_config));
}

TEST(FakeUserDefinedOutputPluginFactory, FactoryRegistersUnderCorrectName) {
  FakeUserDefinedOutputConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.fake_user_defined_output");
  EXPECT_EQ(factory.name(), "nighthawk.fake_user_defined_output");
}

TEST(FakeUserDefinedOutputPluginFactory, CreateUserDefinedOutputPluginCreatesCorrectPluginType) {
  FakeUserDefinedOutputConfig config;
  Envoy::ProtobufWkt::Any config_any;
  config_any.PackFrom(config);
  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.fake_user_defined_output");
  absl::StatusOr<UserDefinedOutputPluginPtr> plugin =
      factory.createUserDefinedOutputPlugin(config_any, {});
  ASSERT_TRUE(plugin.ok());
  EXPECT_NE(dynamic_cast<FakeUserDefinedOutputPlugin*>(plugin->get()), nullptr);
}

TEST(GetPerWorkerOutput, ReturnsProtoOfCorrectType) {
  absl::StatusOr<UserDefinedOutputPluginPtr> plugin = CreatePlugin("", /*worker_number=*/0);
  ASSERT_TRUE(plugin.ok());

  absl::StatusOr<Envoy::ProtobufWkt::Any> any_or = (*plugin)->getPerWorkerOutput();
  EXPECT_TRUE(any_or.status().ok());
  EXPECT_TRUE(any_or->Is<FakeUserDefinedOutput>());
}

TEST(GetPerWorkerOutput, ReturnsCorrectWorkerNumber) {
  absl::StatusOr<UserDefinedOutputPluginPtr> plugin = CreatePlugin("", /*worker_number=*/13);
  ASSERT_TRUE(plugin.ok());

  Envoy::ProtobufWkt::Any expected_output = CreateOutputAny(R"pb(
    worker_name: "worker_13"
  )pb");

  absl::StatusOr<Envoy::ProtobufWkt::Any> any_or = (*plugin)->getPerWorkerOutput();
  EXPECT_THAT(*any_or, EqualsProto(expected_output));
}

TEST(GetPerWorkerOutput, FailsIfConfiguredToFail) {
  absl::StatusOr<UserDefinedOutputPluginPtr> plugin =
      CreatePlugin("fail_per_worker_output: true", /*worker_number=*/13);
  ASSERT_TRUE(plugin.ok());

  absl::StatusOr<Envoy::ProtobufWkt::Any> any_or = (*plugin)->getPerWorkerOutput();
  EXPECT_EQ(any_or.status().code(), absl::StatusCode::kInternal);
}

TEST(HandleResponseHeaders, IncrementsHeadersCalledCount) {
  absl::StatusOr<UserDefinedOutputPluginPtr> plugin = CreatePlugin("", /*worker_number=*/0);
  ASSERT_TRUE(plugin.ok());
  TestResponseHeaderMapImpl headers{};
  EXPECT_TRUE((*plugin)->handleResponseHeaders(headers).ok());
  EXPECT_TRUE((*plugin)->handleResponseHeaders(headers).ok());

  Envoy::ProtobufWkt::Any expected_output = CreateOutputAny(R"pb(
    headers_called: 2
    worker_name: "worker_0"
  )pb");

  absl::StatusOr<Envoy::ProtobufWkt::Any> any_or = (*plugin)->getPerWorkerOutput();
  EXPECT_THAT(*any_or, EqualsProto(expected_output));
}

TEST(HandleResponseHeaders, FailsAfterCorrectIterationsIfConfigured) {
  absl::StatusOr<UserDefinedOutputPluginPtr> plugin =
      CreatePlugin("fail_headers: true   header_failure_countdown: 2", /*worker_number=*/0);
  ASSERT_TRUE(plugin.ok());
  TestResponseHeaderMapImpl headers{};
  EXPECT_TRUE((*plugin)->handleResponseHeaders(headers).ok());
  EXPECT_TRUE((*plugin)->handleResponseHeaders(headers).ok());
  EXPECT_EQ((*plugin)->handleResponseHeaders(headers).code(), absl::StatusCode::kInternal);
}

TEST(HandleResponseData, IncrementsDataCalledCountIfNotEmpty) {
  absl::StatusOr<UserDefinedOutputPluginPtr> plugin = CreatePlugin("", /*worker_number=*/0);
  ASSERT_TRUE(plugin.ok());
  Envoy::MockBuffer filled_buffer;
  filled_buffer.add("notempty");
  Envoy::MockBuffer empty_buffer;
  EXPECT_TRUE((*plugin)->handleResponseData(filled_buffer).ok());
  EXPECT_TRUE((*plugin)->handleResponseData(filled_buffer).ok());
  EXPECT_TRUE((*plugin)->handleResponseData(empty_buffer).ok());
  EXPECT_TRUE((*plugin)->handleResponseData(empty_buffer).ok());

  Envoy::ProtobufWkt::Any expected_output = CreateOutputAny(R"pb(
    data_called: 2
    worker_name: "worker_0"
  )pb");

  absl::StatusOr<Envoy::ProtobufWkt::Any> any_or = (*plugin)->getPerWorkerOutput();
  EXPECT_THAT(*any_or, EqualsProto(expected_output));
}

TEST(HandleResponseData, FailsAfterCorrectIterationsIfConfigured) {
  absl::StatusOr<UserDefinedOutputPluginPtr> plugin =
      CreatePlugin("fail_data: true   data_failure_countdown: 2", /*worker_number=*/0);
  ASSERT_TRUE(plugin.ok());
  Envoy::MockBuffer buffer;
  buffer.add("notempty");
  EXPECT_TRUE((*plugin)->handleResponseData(buffer).ok());
  EXPECT_TRUE((*plugin)->handleResponseData(buffer).ok());
  EXPECT_EQ((*plugin)->handleResponseData(buffer).code(), absl::StatusCode::kInternal);
}

TEST(AggregateGlobalOutput, BuildsOutputsCorrectly) {
  std::vector<nighthawk::client::UserDefinedOutput> per_worker_outputs({
      CreateUserDefinedOutput(R"pb(
    data_called: 1
    headers_called: 3
    worker_name: "worker_0"
  )pb"),
      CreateUserDefinedOutput(R"pb(
    data_called: 5
    headers_called: 7
    worker_name: "worker_1"
  )pb"),
  });

  Envoy::ProtobufWkt::Any expected_aggregate = CreateOutputAny(R"pb(
    data_called: 6
    headers_called: 10
    worker_name: "global"
  )pb");

  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.fake_user_defined_output");
  absl::StatusOr<Envoy::ProtobufWkt::Any> any_or =
      factory.AggregateGlobalOutput(per_worker_outputs);

  ASSERT_TRUE(any_or.status().ok());
  EXPECT_THAT(*any_or, EqualsProto(expected_aggregate));
}

TEST(AggregateGlobalOutput, FailsElegantlyWithIncorrectInput) {
  Envoy::ProtobufWkt::Any invalid_any;
  FakeUserDefinedOutputConfig wrong_type;
  invalid_any.PackFrom(wrong_type);
  nighthawk::client::UserDefinedOutput user_defined_output;
  *user_defined_output.mutable_typed_output() = invalid_any;
  std::vector<nighthawk::client::UserDefinedOutput> per_worker_outputs = {user_defined_output};

  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<UserDefinedOutputPluginFactory>(
      "nighthawk.fake_user_defined_output");
  absl::StatusOr<Envoy::ProtobufWkt::Any> any_or =
      factory.AggregateGlobalOutput(per_worker_outputs);

  EXPECT_EQ(any_or.status().code(), absl::StatusCode::kInternal);
}

} // namespace
} // namespace Nighthawk
