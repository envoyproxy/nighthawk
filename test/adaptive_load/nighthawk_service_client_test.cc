#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/options.pb.h"
#include "api/client/service.grpc.pb.h"
#include "api/client/service_mock.grpc.pb.h"

#include "grpcpp/test/mock_stream.h"

#include "adaptive_load/nighthawk_service_client.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

using ::Envoy::Protobuf::util::MessageDifferencer;
using ::nighthawk::client::CommandLineOptions;
using ::nighthawk::client::ExecutionRequest;
using ::nighthawk::client::ExecutionResponse;
using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;

TEST(PerformNighthawkBenchmark, UsesSpecifiedDuration) {
  const int kExpectedSeconds = 123;
  ExecutionRequest request;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillOnce([&request](grpc_impl::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<ExecutionRequest, ExecutionResponse>();
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillOnce(Return(true)).WillOnce(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillOnce(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish()).WillOnce(Return(::grpc::Status::OK));
        return mock_reader_writer;
      });
  Envoy::Protobuf::Duration duration;
  duration.set_seconds(kExpectedSeconds);
  absl::StatusOr<ExecutionResponse> response_or =
      PerformNighthawkBenchmark(&mock_nighthawk_service_stub, CommandLineOptions(), duration);
  EXPECT_TRUE(response_or.ok());
  EXPECT_EQ(request.start_request().options().duration().seconds(), kExpectedSeconds);
}

TEST(PerformNighthawkBenchmark, UsesSpecifiedCommandLineOptions) {
  const int kExpectedRps = 456;
  ExecutionRequest request;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillOnce([&request](grpc_impl::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<ExecutionRequest, ExecutionResponse>();
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillOnce(Return(true)).WillOnce(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillOnce(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish()).WillOnce(Return(::grpc::Status::OK));
        return mock_reader_writer;
      });
  CommandLineOptions command_line_options;
  command_line_options.mutable_requests_per_second()->set_value(kExpectedRps);
  absl::StatusOr<ExecutionResponse> response_or = PerformNighthawkBenchmark(
      &mock_nighthawk_service_stub, command_line_options, Envoy::Protobuf::Duration());
  EXPECT_TRUE(response_or.ok());
  EXPECT_EQ(request.start_request().options().requests_per_second().value(), kExpectedRps);
}

TEST(PerformNighthawkBenchmark, ReturnsNighthawkResponseSuccessfully) {
  ExecutionResponse expected_response;
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillOnce([&expected_response](grpc_impl::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<ExecutionRequest, ExecutionResponse>();
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillOnce(DoAll(SetArgPointee<0>(expected_response), Return(true)))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish()).WillOnce(Return(::grpc::Status::OK));
        return mock_reader_writer;
      });
  absl::StatusOr<ExecutionResponse> response_or = PerformNighthawkBenchmark(
      &mock_nighthawk_service_stub, CommandLineOptions(), Envoy::Protobuf::Duration());
  EXPECT_TRUE(response_or.ok());
  ExecutionResponse actual_response = response_or.value();
  EXPECT_TRUE(MessageDifferencer::Equivalent(actual_response, expected_response));
  EXPECT_EQ(actual_response.DebugString(), expected_response.DebugString());
}

TEST(PerformNighthawkBenchmark, ReturnsErrorIfNighthawkServiceDoesNotSendResponse) {
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillOnce([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<ExecutionRequest, ExecutionResponse>();
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillOnce(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        return mock_reader_writer;
      });
  absl::StatusOr<ExecutionResponse> response_or = PerformNighthawkBenchmark(
      &mock_nighthawk_service_stub, CommandLineOptions(), Envoy::Protobuf::Duration());
  ASSERT_FALSE(response_or.ok());
  EXPECT_EQ(response_or.status().code(), absl::StatusCode::kUnknown);
  EXPECT_THAT(response_or.status().message(),
              HasSubstr("Nighthawk Service did not send a gRPC response."));
}

TEST(PerformNighthawkBenchmark, ReturnsErrorIfNighthawkServiceWriteFails) {
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillOnce([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<ExecutionRequest, ExecutionResponse>();
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillOnce(Return(false));
        return mock_reader_writer;
      });
  absl::StatusOr<ExecutionResponse> response_or = PerformNighthawkBenchmark(
      &mock_nighthawk_service_stub, CommandLineOptions(), Envoy::Protobuf::Duration());
  ASSERT_FALSE(response_or.ok());
  EXPECT_EQ(response_or.status().code(), absl::StatusCode::kUnknown);
  EXPECT_THAT(response_or.status().message(), HasSubstr("Failed to write"));
}

TEST(PerformNighthawkBenchmark, ReturnsErrorIfNighthawkServiceWritesDoneFails) {
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillOnce([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<ExecutionRequest, ExecutionResponse>();
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(false));
        return mock_reader_writer;
      });
  absl::StatusOr<ExecutionResponse> response_or = PerformNighthawkBenchmark(
      &mock_nighthawk_service_stub, CommandLineOptions(), Envoy::Protobuf::Duration());
  ASSERT_FALSE(response_or.ok());
  EXPECT_EQ(response_or.status().code(), absl::StatusCode::kUnknown);
  EXPECT_THAT(response_or.status().message(), HasSubstr("WritesDone() failed"));
}

TEST(PerformNighthawkBenchmark, ReturnsErrorIfNighthawkServiceGrpcStreamClosesAbnormally) {
  nighthawk::client::MockNighthawkServiceStub mock_nighthawk_service_stub;
  EXPECT_CALL(mock_nighthawk_service_stub, ExecutionStreamRaw)
      .WillOnce([](grpc_impl::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<ExecutionRequest, ExecutionResponse>();
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillOnce(Return(true)).WillOnce(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish())
            .WillOnce(Return(::grpc::Status(::grpc::UNKNOWN, "Finish failure status message")));
        return mock_reader_writer;
      });
  absl::StatusOr<ExecutionResponse> response_or = PerformNighthawkBenchmark(
      &mock_nighthawk_service_stub, CommandLineOptions(), Envoy::Protobuf::Duration());
  ASSERT_FALSE(response_or.ok());
  EXPECT_EQ(response_or.status().code(), absl::StatusCode::kUnknown);
  EXPECT_THAT(response_or.status().message(), HasSubstr("Finish failure status message"));
}

} // namespace
} // namespace Nighthawk
