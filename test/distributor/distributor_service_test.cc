#include <grpc++/grpc++.h>

#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/utility.h"

#include "api/distributor/distributor.pb.h"

#include "distributor/service_impl.h"

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
    service_ = std::make_unique<NighthawkDistributorServiceImpl>();
    grpc::ServerBuilder builder;
    loopback_address_ = Envoy::Network::Test::getLoopbackAddressUrlString(GetParam());

    builder.AddListeningPort(fmt::format("{}:0", loopback_address_),
                             grpc::InsecureServerCredentials(), &grpc_server_port_);
    builder.RegisterService(service_.get());
    server_ = builder.BuildAndStart();
    setupGrpcClient();
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

INSTANTIATE_TEST_SUITE_P(IpVersions, DistributorServiceTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(DistributorServiceTest, NoExecutionRequestFails) {
  std::unique_ptr<grpc::ClientReaderWriter<DistributedRequest, DistributedResponse>> reader_writer =
      stub_->DistributedRequestStream(&context_);
  reader_writer->Write(request_, {});
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
  request_.mutable_execution_request();
  reader_writer->Write(request_, {});
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_FALSE(reader_writer->Read(&response_));
  auto status = reader_writer->Finish();
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_THAT(status.error_message(),
              HasSubstr("DistributedRequest.ExecutionRequest MUST specify one or more services"));
}

TEST_P(DistributorServiceTest, NoStartRequestSpecifiedFails) {
  std::unique_ptr<grpc::ClientReaderWriter<DistributedRequest, DistributedResponse>> reader_writer =
      stub_->DistributedRequestStream(&context_);
  request_.mutable_execution_request();
  request_.add_services();
  reader_writer->Write(request_, {});
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_FALSE(reader_writer->Read(&response_));
  auto status = reader_writer->Finish();
  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
  EXPECT_THAT(status.error_message(),
              HasSubstr("DistributedRequest.ExecutionRequest MUST have StartRequest"));
}

TEST_P(DistributorServiceTest, NoOptionsForStartRequestFails) {
  std::unique_ptr<grpc::ClientReaderWriter<DistributedRequest, DistributedResponse>> reader_writer =
      stub_->DistributedRequestStream(&context_);
  request_.add_services();
  ExecutionRequest* execution_request = request_.mutable_execution_request();
  execution_request->mutable_start_request();
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
  request_.add_services();
  ExecutionRequest* execution_request = request_.mutable_execution_request();
  execution_request->mutable_start_request()->mutable_options();
  reader_writer->Write(request_, {});
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_TRUE(reader_writer->Read(&response_));
  auto status = reader_writer->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("One or more execution requests failed"));
  ASSERT_EQ(response_.fragment_size(), 1);
  EXPECT_TRUE(response_.fragment(0).has_error());
  EXPECT_EQ(response_.fragment(0).error().code(), grpc::StatusCode::UNAVAILABLE);
  EXPECT_THAT(response_.fragment(0).error().message(),
              HasSubstr("Distributed Execution Request failed: Failed to write request to the "
                        "Nighthawk Service gRPC channel"));
}

} // namespace
} // namespace Nighthawk