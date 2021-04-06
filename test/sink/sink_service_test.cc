#include <grpc++/grpc++.h>

#include <vector>

#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/utility.h"

#include "api/sink/sink.pb.h"

#include "sink/service_impl.h"

#include "test/mocks/sink/mock_sink.h"

#include "absl/synchronization/notification.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::nighthawk::SinkRequest;
using ::nighthawk::SinkResponse;
using ::nighthawk::StoreExecutionRequest;
using ::nighthawk::StoreExecutionResponse;
using ::nighthawk::client::ExecutionResponse;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

class SinkServiceTest : public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  void SetUp() override {
    auto sink = std::make_unique<MockSink>();
    sink_ = sink.get();
    service_ = std::make_unique<SinkServiceImpl>(std::move(sink));
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
    stub_ = std::make_unique<nighthawk::NighthawkSink::Stub>(channel_);
  }

  MockSink* sink_;
  std::unique_ptr<SinkServiceImpl> service_;
  std::unique_ptr<grpc::Server> server_;
  std::shared_ptr<grpc::Channel> channel_;
  grpc::ClientContext context_;
  SinkRequest request_;
  SinkResponse response_;
  std::unique_ptr<nighthawk::NighthawkSink::Stub> stub_;
  std::string loopback_address_;
  int grpc_server_port_{0};
};

INSTANTIATE_TEST_SUITE_P(IpVersions, SinkServiceTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(SinkServiceTest, LoadSingleResultWithJustExecutionResponse) {
  // Our mock sink will yield a single result, with the correct execution id.
  const std::string kTestId = "test-id";
  absl::StatusOr<std::vector<ExecutionResponse>> response_from_mock_sink =
      std::vector<ExecutionResponse>{{}};
  ExecutionResponse& response = response_from_mock_sink.value().at(0);
  response.set_execution_id(kTestId);
  response.mutable_output();
  request_.set_execution_id(kTestId);
  std::unique_ptr<grpc::ClientReaderWriter<SinkRequest, SinkResponse>> reader_writer =
      stub_->SinkRequestStream(&context_);
  EXPECT_CALL(*sink_, LoadExecutionResult(kTestId)).WillOnce(Return(response_from_mock_sink));
  reader_writer->Write(request_, {});
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_TRUE(reader_writer->Read(&response_));
  EXPECT_TRUE(response_.has_execution_response());
  EXPECT_EQ(response_.execution_response().execution_id(), kTestId);
  EXPECT_TRUE(reader_writer->Finish().ok());
}

TEST_P(SinkServiceTest, LoadSingleSinkYieldsWrongExecutionId) {
  // Our mock sink will yield a single result, but with a wrong/unexpected execution id.
  const std::string kTestId = "test-id";
  absl::StatusOr<std::vector<ExecutionResponse>> response_from_mock_sink =
      std::vector<ExecutionResponse>{{}};
  response_from_mock_sink.value().at(0).set_execution_id("wrong-id");
  request_.set_execution_id(kTestId);
  std::unique_ptr<grpc::ClientReaderWriter<SinkRequest, SinkResponse>> reader_writer =
      stub_->SinkRequestStream(&context_);
  EXPECT_CALL(*sink_, LoadExecutionResult(kTestId)).WillOnce(Return(response_from_mock_sink));
  reader_writer->Write(request_, {});
  EXPECT_TRUE(reader_writer->WritesDone());
  EXPECT_FALSE(reader_writer->Read(&response_));
  grpc::Status status = reader_writer->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.error_message(), "INTERNAL: Expected execution_id 'test-id' got 'wrong-id'");
}

TEST_P(SinkServiceTest, LoadSingleSinkYieldsEmptyResultSet) {
  // Our mock sink will yield an empty vector of results.
  const std::string kTestId = "test-id";
  absl::StatusOr<std::vector<ExecutionResponse>> response_from_mock_sink =
      std::vector<ExecutionResponse>{};
  request_.set_execution_id(kTestId);
  std::unique_ptr<grpc::ClientReaderWriter<SinkRequest, SinkResponse>> reader_writer =
      stub_->SinkRequestStream(&context_);
  EXPECT_CALL(*sink_, LoadExecutionResult(kTestId)).WillOnce(Return(response_from_mock_sink));
  reader_writer->Write(request_, {});
  EXPECT_TRUE(reader_writer->WritesDone());
  EXPECT_FALSE(reader_writer->Read(&response_));
  grpc::Status status = reader_writer->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.error_message(), "NOT_FOUND: No results");
}

TEST_P(SinkServiceTest, LoadTwoResultsWithExecutionResponseWhereOneHasErrorDetails) {
  // Set up the mock sink to yield two results on the call to load, both with execution results
  // attached. One of the execution results will have an error detail set, indicating that remote
  // execution didn't terminate successfully.
  const std::string kTestId = "test-id";
  absl::StatusOr<std::vector<ExecutionResponse>> response_from_mock_sink =
      std::vector<ExecutionResponse>{{}, {}};
  ExecutionResponse& response = response_from_mock_sink.value().at(0);
  response.set_execution_id(kTestId);
  response.mutable_output();
  ExecutionResponse& response_2 = response_from_mock_sink.value().at(1);
  response_2.set_execution_id(kTestId);
  response_2.mutable_output();
  google::rpc::Status* error_detail = response.mutable_error_detail();
  error_detail->set_code(-5);
  error_detail->set_message("foo error");

  request_.set_execution_id(kTestId);

  std::unique_ptr<grpc::ClientReaderWriter<SinkRequest, SinkResponse>> reader_writer =
      stub_->SinkRequestStream(&context_);
  EXPECT_CALL(*sink_, LoadExecutionResult(kTestId)).WillOnce(Return(response_from_mock_sink));
  reader_writer->Write(request_, {});
  EXPECT_TRUE(reader_writer->WritesDone());

  // Make sure that the response we get reflects what the mock sink's Load call returned.
  ASSERT_TRUE(reader_writer->Read(&response_));
  EXPECT_TRUE(response_.has_execution_response());
  EXPECT_EQ(response_.execution_response().execution_id(), kTestId);
  ASSERT_TRUE(response_.execution_response().has_error_detail());
  EXPECT_EQ(response_.execution_response().error_detail().code(), -1);
  EXPECT_EQ(response_.execution_response().error_detail().message(),
            "One or more remote execution(s) terminated with a failure.");
  ASSERT_EQ(response_.execution_response().error_detail().details_size(), 1);
  ASSERT_TRUE(response_.execution_response().error_detail().details(0).Is<::google::rpc::Status>());
  ::google::rpc::Status status;
  Envoy::MessageUtil::unpackTo(response_.execution_response().error_detail().details(0), status);
  // TODO(XXX): proper equivalence test.
  EXPECT_EQ(status.DebugString(), error_detail->DebugString());
  EXPECT_TRUE(reader_writer->Finish().ok());
}

TEST_P(SinkServiceTest, LoadWhenSinkYieldsFailureStatus) {
  absl::StatusOr<std::vector<ExecutionResponse>> response_from_mock_sink =
      absl::InvalidArgumentError("test");
  std::unique_ptr<grpc::ClientReaderWriter<SinkRequest, SinkResponse>> reader_writer =
      stub_->SinkRequestStream(&context_);
  EXPECT_CALL(*sink_, LoadExecutionResult(_)).WillOnce(Return(response_from_mock_sink));
  reader_writer->Write(request_, {});
  EXPECT_TRUE(reader_writer->WritesDone());
  EXPECT_FALSE(reader_writer->Read(&response_));
  grpc::Status status = reader_writer->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.error_message(), "INVALID_ARGUMENT: test");
}

TEST_P(SinkServiceTest, ResultWriteFailure) {
  // This test covers the flow where the gRPC service fails while writing a reply message to the
  // stream. We don't have any expectations other then that the service doesn't crash in that flow.
  std::unique_ptr<grpc::ClientReaderWriter<SinkRequest, SinkResponse>> reader_writer =
      stub_->SinkRequestStream(&context_);
  absl::Notification notification;
  EXPECT_CALL(*sink_, LoadExecutionResult(_))
      .WillOnce(testing::DoAll(Invoke([&notification]() { notification.Notify(); }),
                               Return(std::vector<ExecutionResponse>{{}, {}})));
  EXPECT_TRUE(reader_writer->Write(request_, {}));
  // Wait for the expected invokation to avoid a race with test execution end.
  notification.WaitForNotification();
  context_.TryCancel();
}

TEST_P(SinkServiceTest, LoadWithOutputMergeFailure) {
  // This tests sets up a sink response with outputs that will fail to merge, in order
  // to trigger the flow to handle that in the gRPC service.
  const std::string kTestId = "test-id";
  absl::StatusOr<std::vector<ExecutionResponse>> response_from_mock_sink =
      std::vector<ExecutionResponse>{{}, {}};
  ExecutionResponse& response = response_from_mock_sink.value().at(0);
  response.set_execution_id(kTestId);
  response.mutable_output();
  nighthawk::client::CommandLineOptions* options_1 = response.mutable_output()->mutable_options();
  options_1->mutable_requests_per_second()->set_value(1);
  ExecutionResponse& response_2 = response_from_mock_sink.value().at(1);
  response_2.set_execution_id(kTestId);
  response_2.mutable_output();
  request_.set_execution_id(kTestId);
  nighthawk::client::CommandLineOptions* options_2 = response_2.mutable_output()->mutable_options();
  options_2->mutable_requests_per_second()->set_value(2);
  std::unique_ptr<grpc::ClientReaderWriter<SinkRequest, SinkResponse>> reader_writer =
      stub_->SinkRequestStream(&context_);
  EXPECT_CALL(*sink_, LoadExecutionResult(kTestId)).WillOnce(Return(response_from_mock_sink));
  reader_writer->Write(request_, {});
  EXPECT_TRUE(reader_writer->WritesDone());
  ASSERT_FALSE(reader_writer->Read(&response_));
  EXPECT_FALSE(response_.has_execution_response());
  grpc::Status status = reader_writer->Finish();
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("INTERNAL: Options divergence detected"));
}

TEST_P(SinkServiceTest, StoreExecutionResponseStreamOK) {
  StoreExecutionResponse response;
  ExecutionResponse result_to_store;
  std::unique_ptr<::grpc::ClientWriter<::nighthawk::StoreExecutionRequest>> writer =
      stub_->StoreExecutionResponseStream(&context_, &response);
  EXPECT_CALL(*sink_, StoreExecutionResultPiece(_))
      .WillOnce(Return(absl::OkStatus()))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_TRUE(writer->Write({}));
  EXPECT_TRUE(writer->Write({}));
  EXPECT_TRUE(writer->WritesDone());
  grpc::Status status = writer->Finish();
  EXPECT_TRUE(status.ok());
}

TEST_P(SinkServiceTest, StoreExecutionResponseStreamFailure) {
  StoreExecutionResponse response;
  ExecutionResponse result_to_store;
  std::unique_ptr<::grpc::ClientWriter<::nighthawk::StoreExecutionRequest>> writer =
      stub_->StoreExecutionResponseStream(&context_, &response);
  EXPECT_CALL(*sink_, StoreExecutionResultPiece(_))
      .WillOnce(Return(absl::InvalidArgumentError("test")));
  EXPECT_TRUE(writer->Write({}));
  EXPECT_TRUE(writer->WritesDone());
  grpc::Status status = writer->Finish();
  EXPECT_FALSE(status.ok());
}

TEST_P(SinkServiceTest, StoreExecutionResponseStreamNullReader) {
  StoreExecutionResponse response;
  ExecutionResponse result_to_store;
  std::unique_ptr<::grpc::ClientWriter<::nighthawk::StoreExecutionRequest>> writer =
      stub_->StoreExecutionResponseStream(&context_, &response);
  EXPECT_CALL(*sink_, StoreExecutionResultPiece(_))
      .WillOnce(Return(absl::InvalidArgumentError("test")));
  EXPECT_TRUE(writer->Write({}));
  EXPECT_TRUE(writer->WritesDone());
  grpc::Status status = writer->Finish();
  EXPECT_FALSE(status.ok());
}

TEST(ResponseVectorHandling, EmptyVectorYieldsNotOK) {
  std::vector<ExecutionResponse> responses;
  absl::StatusOr<ExecutionResponse> response =
      mergeExecutionResponses(/*execution_id=*/"foo", responses);
  EXPECT_FALSE(response.ok());
}

TEST(ResponseVectorHandling, NoResultsInOutputYieldsNone) {
  ExecutionResponse result;
  std::vector<ExecutionResponse> responses{result, result, result};
  absl::StatusOr<ExecutionResponse> response =
      mergeExecutionResponses(/*execution_id=*/"", responses);
  EXPECT_TRUE(response.ok());
  EXPECT_EQ(response.value().output().results().size(), 0);
}

TEST(ResponseVectorHandling, MergeThreeYieldsThree) {
  ExecutionResponse result;
  result.mutable_output()->add_results();
  std::vector<ExecutionResponse> responses{result, result, result};
  absl::StatusOr<ExecutionResponse> response =
      mergeExecutionResponses(/*execution_id=*/"", responses);
  EXPECT_TRUE(response.ok());
  EXPECT_EQ(response.value().output().results().size(), 3);
}

TEST(MergeOutputs, MergeDivergingOptionsInResultsFails) {
  std::vector<ExecutionResponse> responses;
  nighthawk::client::Output output_1;
  nighthawk::client::CommandLineOptions* options_1 = output_1.mutable_options();
  options_1->mutable_requests_per_second()->set_value(1);
  nighthawk::client::Output output_2;
  nighthawk::client::CommandLineOptions* options_2 = output_2.mutable_options();
  options_2->mutable_requests_per_second()->set_value(2);
  nighthawk::client::Output merged_output;
  absl::Status status_1 = mergeOutput(output_1, merged_output);
  EXPECT_TRUE(status_1.ok());
  absl::Status status_2 = mergeOutput(output_2, merged_output);
  EXPECT_FALSE(status_2.ok());
  EXPECT_THAT(status_2.message(), HasSubstr("Options divergence detected"));
}

TEST(MergeOutputs, MergeDivergingVersionsInResultsFails) {
  const std::string kTestId = "test-id";
  std::vector<ExecutionResponse> responses;
  ExecutionResponse response_1;
  response_1.set_execution_id(kTestId);
  response_1.mutable_output()->mutable_version()->mutable_version()->set_major_number(1);
  ExecutionResponse response_2;
  response_2.set_execution_id(kTestId);
  response_2.mutable_output()->mutable_version()->mutable_version()->set_major_number(2);
  nighthawk::client::Output merged_output;
  absl::Status status_1 = mergeOutput(response_1.output(), merged_output);
  EXPECT_TRUE(status_1.ok());
  absl::Status status_2 = mergeOutput(response_2.output(), merged_output);
  EXPECT_FALSE(status_2.ok());
  EXPECT_THAT(status_2.message(), HasSubstr("Version divergence detected"));
}

} // namespace
} // namespace Nighthawk
