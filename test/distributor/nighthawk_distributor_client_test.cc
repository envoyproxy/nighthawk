#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/client/options.pb.h"
#include "api/distributor/distributor.grpc.pb.h"
#include "api/distributor/distributor_mock.grpc.pb.h"

#include "distributor/nighthawk_distributor_client_impl.h"

#include "grpcpp/test/mock_stream.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

namespace {

using ::Envoy::Protobuf::util::MessageDifferencer;
using ::nighthawk::DistributedRequest;
using ::nighthawk::DistributedResponse;
using ::nighthawk::client::CommandLineOptions;
using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;

TEST(DistributedRequest, UsesSpecifiedCommandLineOptions) {
  const int kExpectedRps = 456;
  DistributedRequest request;
  nighthawk::MockNighthawkDistributorStub mock_nighthawk_service_stub;
  // Configure the mock Nighthawk Service stub to return an inner mock channel when the code under
  // test requests a channel. Set call expectations on the inner mock channel.
  EXPECT_CALL(mock_nighthawk_service_stub, DistributedRequestStreamRaw)
      .WillOnce([&request](grpc::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<DistributedRequest, DistributedResponse>();
        // DistributedRequest currently expects Read to return true exactly once.
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillOnce(Return(true)).WillOnce(Return(false));
        // Capture the Nighthawk request DistributedRequest sends on the channel.
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillOnce(::testing::DoAll(::testing::SaveArg<0>(&request), Return(true)));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish()).WillOnce(Return(::grpc::Status::OK));
        return mock_reader_writer;
      });

  ::nighthawk::DistributedRequest distributed_request;
  ::nighthawk::client::ExecutionRequest execution_request;
  ::nighthawk::client::StartRequest start_request;
  CommandLineOptions command_line_options;
  command_line_options.mutable_requests_per_second()->set_value(kExpectedRps);
  *(start_request.mutable_options()) = command_line_options;
  *(execution_request.mutable_start_request()) = start_request;
  *(distributed_request.mutable_execution_request()) = execution_request;
  NighthawkDistributorClientImpl client;
  absl::StatusOr<DistributedResponse> distributed_response_or =
      client.DistributedRequest(mock_nighthawk_service_stub, distributed_request);
  EXPECT_TRUE(distributed_response_or.ok());
  ASSERT_TRUE(request.has_execution_request());
  ASSERT_TRUE(request.execution_request().has_start_request());
  ASSERT_TRUE(request.execution_request().start_request().has_options());
  EXPECT_EQ(request.execution_request().start_request().options().requests_per_second().value(),
            kExpectedRps);
}

TEST(DistributedRequest, ReturnsNighthawkResponseSuccessfully) {
  DistributedResponse expected_response;
  nighthawk::MockNighthawkDistributorStub mock_nighthawk_service_stub;
  // Configure the mock Nighthawk Service stub to return an inner mock channel when the code under
  // test requests a channel. Set call expectations on the inner mock channel.
  EXPECT_CALL(mock_nighthawk_service_stub, DistributedRequestStreamRaw)
      .WillOnce([&expected_response](grpc::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<DistributedRequest, DistributedResponse>();
        // DistributedRequest currently expects Read to return true exactly once.
        // Capture the gRPC response proto as it is written to the output parameter.
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillOnce(DoAll(SetArgPointee<0>(expected_response), Return(true)))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish()).WillOnce(Return(::grpc::Status::OK));
        return mock_reader_writer;
      });

  NighthawkDistributorClientImpl client;
  absl::StatusOr<DistributedResponse> response_or =
      client.DistributedRequest(mock_nighthawk_service_stub, ::nighthawk::DistributedRequest());
  EXPECT_TRUE(response_or.ok());
  DistributedResponse actual_response = response_or.value();
  EXPECT_TRUE(MessageDifferencer::Equivalent(actual_response, expected_response));
  EXPECT_EQ(actual_response.DebugString(), expected_response.DebugString());
}

TEST(DistributedRequest, ReturnsErrorIfNighthawkServiceDoesNotSendResponse) {
  nighthawk::MockNighthawkDistributorStub mock_nighthawk_service_stub;
  // Configure the mock Nighthawk Service stub to return an inner mock channel when the code under
  // test requests a channel. Set call expectations on the inner mock channel.
  EXPECT_CALL(mock_nighthawk_service_stub, DistributedRequestStreamRaw)
      .WillOnce([](grpc::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<DistributedRequest, DistributedResponse>();
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillOnce(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        return mock_reader_writer;
      });

  NighthawkDistributorClientImpl client;
  absl::StatusOr<DistributedResponse> response_or =
      client.DistributedRequest(mock_nighthawk_service_stub, ::nighthawk::DistributedRequest());
  ASSERT_FALSE(response_or.ok());
  EXPECT_EQ(response_or.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(response_or.status().message(),
              HasSubstr("Distributor Service did not send a gRPC response."));
}

TEST(DistributedRequest, ReturnsErrorIfNighthawkServiceWriteFails) {
  nighthawk::MockNighthawkDistributorStub mock_nighthawk_service_stub;
  // Configure the mock Nighthawk Service stub to return an inner mock channel when the code under
  // test requests a channel. Set call expectations on the inner mock channel.
  EXPECT_CALL(mock_nighthawk_service_stub, DistributedRequestStreamRaw)
      .WillOnce([](grpc::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<DistributedRequest, DistributedResponse>();
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillOnce(Return(false));
        return mock_reader_writer;
      });

  NighthawkDistributorClientImpl client;
  absl::StatusOr<DistributedResponse> response_or =
      client.DistributedRequest(mock_nighthawk_service_stub, ::nighthawk::DistributedRequest());
  ASSERT_FALSE(response_or.ok());
  EXPECT_EQ(response_or.status().code(), absl::StatusCode::kUnavailable);
  EXPECT_THAT(response_or.status().message(), HasSubstr("Failed to write"));
}

TEST(DistributedRequest, ReturnsErrorIfNighthawkServiceWritesDoneFails) {
  nighthawk::MockNighthawkDistributorStub mock_nighthawk_service_stub;
  // Configure the mock Nighthawk Service stub to return an inner mock channel when the code under
  // test requests a channel. Set call expectations on the inner mock channel.
  EXPECT_CALL(mock_nighthawk_service_stub, DistributedRequestStreamRaw)
      .WillOnce([](grpc::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<DistributedRequest, DistributedResponse>();
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(false));
        return mock_reader_writer;
      });

  NighthawkDistributorClientImpl client;
  absl::StatusOr<DistributedResponse> response_or =
      client.DistributedRequest(mock_nighthawk_service_stub, ::nighthawk::DistributedRequest());
  ASSERT_FALSE(response_or.ok());
  EXPECT_EQ(response_or.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(response_or.status().message(), HasSubstr("WritesDone() failed"));
}

TEST(DistributedRequest, PropagatesErrorIfNighthawkServiceGrpcStreamClosesAbnormally) {
  nighthawk::MockNighthawkDistributorStub mock_nighthawk_service_stub;
  // Configure the mock Nighthawk Service stub to return an inner mock channel when the code under
  // test requests a channel. Set call expectations on the inner mock channel.
  EXPECT_CALL(mock_nighthawk_service_stub, DistributedRequestStreamRaw)
      .WillOnce([](grpc::ClientContext*) {
        auto* mock_reader_writer =
            new grpc::testing::MockClientReaderWriter<DistributedRequest, DistributedResponse>();
        // DistributedRequest currently expects Read to return true exactly once.
        EXPECT_CALL(*mock_reader_writer, Read(_)).WillOnce(Return(true)).WillOnce(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _)).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish())
            .WillOnce(
                Return(::grpc::Status(::grpc::PERMISSION_DENIED, "Finish failure status message")));
        return mock_reader_writer;
      });

  NighthawkDistributorClientImpl client;
  absl::StatusOr<DistributedResponse> response_or =
      client.DistributedRequest(mock_nighthawk_service_stub, ::nighthawk::DistributedRequest());
  ASSERT_FALSE(response_or.ok());
  EXPECT_EQ(response_or.status().code(), absl::StatusCode::kPermissionDenied);
  EXPECT_THAT(response_or.status().message(), HasSubstr("Finish failure status message"));
}

} // namespace
} // namespace Nighthawk
