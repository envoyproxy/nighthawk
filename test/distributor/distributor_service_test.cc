#include <grpc++/grpc++.h>

#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/utility.h"

#include "api/distributor/distributor.pb.h"

#include "source/common/nighthawk_service_client_impl.h"
#include "source/distributor/service_impl.h"

#include "test/mocks/common/mock_nighthawk_service_client.h"

#include "absl/synchronization/notification.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::nighthawk::DistributedRequest;
using ::nighthawk::DistributedResponse;
using ::nighthawk::client::ExecutionRequest;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

class DistributorServiceTest : public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  void SetUp() override {
    if (service_ == nullptr) {
      service_ = std::make_unique<NighthawkDistributorServiceImpl>(
          std::make_unique<NighthawkServiceClientImpl>());
    }
    grpc::ServerBuilder builder;
    loopback_address_ = Envoy::Network::Test::getLoopbackAddressUrlString(GetParam());

    builder.AddListeningPort(fmt::format("{}:0", loopback_address_),
                             grpc::InsecureServerCredentials(), &grpc_server_port_);
    builder.RegisterService(service_.get());
    server_ = builder.BuildAndStart();
    setupGrpcClient();
    request_.mutable_execution_request()->mutable_start_request();
    envoy::config::core::v3::SocketAddress* socket_address =
        request_.add_services()->mutable_socket_address();
    socket_address->set_address("127.0.0.1");
    socket_address->set_port_value(80);
  }

  void TearDown() override { server_->Shutdown(); }

  void setupGrpcClient() {
    channel_ = grpc::CreateChannel(fmt::format("{}:{}", loopback_address_, grpc_server_port_),
                                   grpc::InsecureChannelCredentials());
    stub_ = std::make_unique<nighthawk::NighthawkDistributor::Stub>(channel_);
  }

  std::unique_ptr<NighthawkDistributorServiceImpl> service_;
  std::unique_ptr<grpc::Server> server_;
  std::shared_ptr<grpc::Channel> channel_;
  grpc::ClientContext context_;
  DistributedRequest request_;
  DistributedResponse response_;
  std::unique_ptr<nighthawk::NighthawkDistributor::Stub> stub_;
  std::string loopback_address_;
  int grpc_server_port_{0};
};

class DistributorServiceWithMockServiceClientTest : public DistributorServiceTest {
public:
  void SetUp() override {
    auto client = std::make_unique<MockNighthawkServiceClient>();
    mock_nighthawk_service_client_ = client.get();
    service_ = std::make_unique<NighthawkDistributorServiceImpl>(std::move(client));
    ON_CALL(*mock_nighthawk_service_client_, PerformNighthawkBenchmark)
        .WillByDefault(Return(nighthawk::client::ExecutionResponse()));
    DistributorServiceTest::SetUp();
  }
  MockNighthawkServiceClient* mock_nighthawk_service_client_;
};

INSTANTIATE_TEST_SUITE_P(IpVersions, DistributorServiceTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(DistributorServiceTest, NoExecutionRequestFails) {
  request_.clear_execution_request();
  std::unique_ptr<grpc::ClientReaderWriter<DistributedRequest, DistributedResponse>> reader_writer =
      stub_->DistributedRequestStream(&context_);
  EXPECT_TRUE(reader_writer->Write(request_, {}));
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_FALSE(reader_writer->Read(&response_));
  auto status = reader_writer->Finish();
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_THAT(status.error_message(),
              HasSubstr("DistributedRequest.ExecutionRequest MUST be specified"));
}

TEST_P(DistributorServiceTest, NoServicesSpecifiedFails) {
  std::unique_ptr<grpc::ClientReaderWriter<DistributedRequest, DistributedResponse>> reader_writer =
      stub_->DistributedRequestStream(&context_);
  request_.clear_services();
  reader_writer->Write(request_, {});
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_FALSE(reader_writer->Read(&response_));
  auto status = reader_writer->Finish();
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_THAT(
      status.error_message(),
      HasSubstr("DistributedRequestValidationError.Services: value must contain at least 1 item"));
}

TEST_P(DistributorServiceTest, NoStartRequestSpecifiedFails) {
  std::unique_ptr<grpc::ClientReaderWriter<DistributedRequest, DistributedResponse>> reader_writer =
      stub_->DistributedRequestStream(&context_);
  request_.mutable_execution_request()->clear_start_request();
  EXPECT_TRUE(reader_writer->Write(request_, {}));
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_FALSE(reader_writer->Read(&response_));
  auto status = reader_writer->Finish();
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_THAT(status.error_message(),
              HasSubstr("embedded message failed validation | caused by field: "
                        "\"command_specific_options\", reason: is required"));
}

TEST_P(DistributorServiceTest, NoOptionsForStartRequestFails) {
  std::unique_ptr<grpc::ClientReaderWriter<DistributedRequest, DistributedResponse>> reader_writer =
      stub_->DistributedRequestStream(&context_);
  reader_writer->Write(request_, {});
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_FALSE(reader_writer->Read(&response_));
  auto status = reader_writer->Finish();
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_THAT(
      status.error_message(),
      HasSubstr("DistributedRequest.ExecutionRequest.StartRequest MUST have CommandLineOptions"));
}

TEST_P(DistributorServiceTest, ValidStartRequestNonExistingServiceYieldsResponseAndGrpcErrorCode) {
  std::unique_ptr<grpc::ClientReaderWriter<DistributedRequest, DistributedResponse>> reader_writer =
      stub_->DistributedRequestStream(&context_);
  ExecutionRequest* execution_request = request_.mutable_execution_request();
  execution_request->mutable_start_request()->mutable_options();
  EXPECT_TRUE(reader_writer->Write(request_, {}));
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_TRUE(reader_writer->Read(&response_));
  auto status = reader_writer->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("One or more execution requests failed"));
  ASSERT_EQ(response_.service_response_size(), 1);
  EXPECT_TRUE(response_.service_response(0).has_error());
  EXPECT_EQ(response_.service_response(0).error().code(), grpc::StatusCode::UNAVAILABLE);
  EXPECT_THAT(response_.service_response(0).error().message(),
              HasSubstr("Distributed Execution Request failed: Failed to write request to the "
                        "Nighthawk Service gRPC channel"));
}

INSTANTIATE_TEST_SUITE_P(IpVersions, DistributorServiceWithMockServiceClientTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(DistributorServiceWithMockServiceClientTest, DistributeToTwoServicesYieldsOk) {
  EXPECT_CALL(*mock_nighthawk_service_client_, PerformNighthawkBenchmark(_, _)).Times(2);
  std::unique_ptr<grpc::ClientReaderWriter<DistributedRequest, DistributedResponse>> reader_writer =
      stub_->DistributedRequestStream(&context_);
  *request_.add_services() = request_.services(0);
  ExecutionRequest* execution_request = request_.mutable_execution_request();
  execution_request->mutable_start_request()->mutable_options();
  EXPECT_TRUE(reader_writer->Write(request_, {}));
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_TRUE(reader_writer->Read(&response_));
  auto status = reader_writer->Finish();
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(response_.service_response_size(), 2);
}

TEST_P(DistributorServiceWithMockServiceClientTest,
       DistributeToSingleServiceErrorReplyYieldsFailure) {
  const std::string kExpectedErrorMessage = "artificial nighthawk service error";
  EXPECT_CALL(*mock_nighthawk_service_client_, PerformNighthawkBenchmark(_, _))
      .WillOnce(Return(absl::DataLossError(kExpectedErrorMessage)));

  std::unique_ptr<grpc::ClientReaderWriter<DistributedRequest, DistributedResponse>> reader_writer =
      stub_->DistributedRequestStream(&context_);
  ExecutionRequest* execution_request = request_.mutable_execution_request();
  execution_request->mutable_start_request()->mutable_options();
  EXPECT_TRUE(reader_writer->Write(request_, {}));
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_TRUE(reader_writer->Read(&response_));
  auto status = reader_writer->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("One or more execution requests failed"));
  ASSERT_EQ(response_.service_response_size(), 1);
  EXPECT_THAT(response_.service_response(0).error().message(),
              HasSubstr("artificial nighthawk service error"));
}

TEST_P(DistributorServiceWithMockServiceClientTest, ServiceSideWriteFailure) {
  // This test covers the flow where the gRPC service fails while writing a reply message to the
  // stream. We don't have any expectations other then that the service doesn't crash in that flow.
  absl::Notification notification;
  EXPECT_CALL(*mock_nighthawk_service_client_, PerformNighthawkBenchmark(_, _))
      .WillOnce(testing::DoAll(Invoke([&notification]() { notification.Notify(); }),
                               Return(nighthawk::client::ExecutionResponse())));
  std::unique_ptr<grpc::ClientReaderWriter<DistributedRequest, DistributedResponse>> reader_writer =
      stub_->DistributedRequestStream(&context_);
  ExecutionRequest* execution_request = request_.mutable_execution_request();
  execution_request->mutable_start_request()->mutable_options();
  EXPECT_TRUE(reader_writer->Write(request_, {}));
  // Wait for the expected invokation to avoid a race with test execution end.
  notification.WaitForNotification();
  context_.TryCancel();
}

} // namespace
} // namespace Nighthawk