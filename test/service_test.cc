#include <grpc++/grpc++.h>

#include <chrono>

#include "nighthawk/common/exception.h"

#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/utility.h"

#include "api/client/service.pb.h"

#include "client/service_impl.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

class ServiceTest : public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  void SetUp() override {
    grpc::ServerBuilder builder;
    loopback_address_ = Envoy::Network::Test::getLoopbackAddressUrlString(GetParam());

    builder.AddListeningPort(fmt::format("{}:0", loopback_address_),
                             grpc::InsecureServerCredentials(), &grpc_server_port_);
    builder.RegisterService(&service_);
    server_ = builder.BuildAndStart();
    setupGrpcClient();
    setBasicRequestOptions();
  }

  void TearDown() override { server_->Shutdown(); }

  void setupGrpcClient() {
    channel_ = grpc::CreateChannel(fmt::format("{}:{}", loopback_address_, grpc_server_port_),
                                   grpc::InsecureChannelCredentials());
    stub_ = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel_);
  }

  void singleStreamBackToBackExecution(grpc::ClientContext& context,
                                       nighthawk::client::NighthawkService::Stub&) {
    auto r = stub_->ExecutionStream(&context);
    EXPECT_TRUE(r->Write(request_, {}));
    EXPECT_TRUE(r->Read(&response_));
    ASSERT_FALSE(response_.has_error_detail());
    EXPECT_TRUE(response_.has_output());
    EXPECT_TRUE(r->Write(request_, {}));
    EXPECT_TRUE(r->Read(&response_));
    EXPECT_FALSE(response_.has_error_detail());
    EXPECT_TRUE(response_.has_output());
    EXPECT_TRUE(r->WritesDone());
    auto status = r->Finish();
    EXPECT_TRUE(status.ok());
  }

  std::thread testThreadedClientRun(bool expect_success) const {
    std::thread thread([this, expect_success]() {
      auto channel = grpc::CreateChannel(fmt::format("{}:{}", loopback_address_, grpc_server_port_),
                                         grpc::InsecureChannelCredentials());
      auto stub = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel);
      grpc::ClientContext context;
      auto stream = stub->ExecutionStream(&context);
      EXPECT_TRUE(stream->Write(request_, {}));
      EXPECT_TRUE(stream->WritesDone());
      nighthawk::client::ExecutionResponse response;
      EXPECT_EQ(stream->Read(&response), expect_success);
      auto status = stream->Finish();
      EXPECT_EQ(status.ok(), expect_success);
    });
    return thread;
  }

  void setBasicRequestOptions() {
    auto options = request_.mutable_start_request()->mutable_options();
    // TODO(oschaaf): this sends actual traffic, which isn't relevant for the tests
    // we are about to perform. However, it would be nice to be able to mock out things
    // to clean this up.
    options->mutable_uri()->set_value("http://127.0.0.1:10001/");
    options->mutable_duration()->set_seconds(2);
    options->mutable_requests_per_second()->set_value(3);
  }

  void runWithFailingValidationExpectations(absl::string_view match_error = "") {
    auto r = stub_->ExecutionStream(&context_);
    r->Write(request_, {});
    r->WritesDone();
    EXPECT_TRUE(r->Read(&response_));
    auto status = r->Finish();
    ASSERT_FALSE(match_error.empty());
    EXPECT_TRUE(response_.has_error_detail());
    EXPECT_FALSE(response_.has_output());
    EXPECT_EQ(::grpc::StatusCode::INTERNAL, response_.error_detail().code());
    EXPECT_THAT(response_.error_detail().message(), HasSubstr(std::string(match_error)));
    EXPECT_TRUE(status.ok());
  }

  ServiceImpl service_;
  std::unique_ptr<grpc::Server> server_;
  std::shared_ptr<grpc::Channel> channel_;
  grpc::ClientContext context_;
  nighthawk::client::ExecutionRequest request_;
  nighthawk::client::ExecutionResponse response_;
  std::unique_ptr<nighthawk::client::NighthawkService::Stub> stub_;
  std::string loopback_address_;
  int grpc_server_port_{0};
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ServiceTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

// Test single NH run
TEST_P(ServiceTest, Basic) {
  auto r = stub_->ExecutionStream(&context_);
  r->Write(request_, {});
  r->WritesDone();
  EXPECT_TRUE(r->Read(&response_));
  ASSERT_FALSE(response_.has_error_detail());
  EXPECT_TRUE(response_.has_output());
  EXPECT_GE(response_.output().results(0).counters().size(), 8);
  auto status = r->Finish();
  EXPECT_TRUE(status.ok());
}

// Test that attempts to perform concurrent executions result in a
// failure being returned.
TEST_P(ServiceTest, NoConcurrentStart) {
  auto r = stub_->ExecutionStream(&context_);
  EXPECT_TRUE(r->Write(request_, {}));
  EXPECT_TRUE(r->Write(request_, {}));
  EXPECT_TRUE(r->WritesDone());
  EXPECT_TRUE(r->Read(&response_));
  ASSERT_FALSE(response_.has_error_detail());
  EXPECT_TRUE(response_.has_output());
  EXPECT_FALSE(r->Read(&response_));
  auto status = r->Finish();
  EXPECT_FALSE(status.ok());
}

// Test we are able to perform serialized executions.
TEST_P(ServiceTest, BackToBackExecution) {
  grpc::ClientContext context1;
  singleStreamBackToBackExecution(context1, *stub_);
  // create a new client to connect to the same server, and do it one more time.
  setupGrpcClient();
  grpc::ClientContext context2;
  singleStreamBackToBackExecution(context2, *stub_);
}

// Test that proto validation is wired up and works.
// TODO(oschaaf): functional coverage of all the options / validations.
TEST_P(ServiceTest, InvalidRps) {
  auto options = request_.mutable_start_request()->mutable_options();
  options->mutable_requests_per_second()->set_value(0);
  runWithFailingValidationExpectations(
      "CommandLineOptionsValidationError.RequestsPerSecond: [\"value must be inside range");
}

// We didn't implement updates yet, ensure we indicate so.
TEST_P(ServiceTest, UpdatesNotSupported) {
  request_ = nighthawk::client::ExecutionRequest();
  request_.mutable_update_request();
  auto r = stub_->ExecutionStream(&context_);
  r->Write(request_, {});
  r->WritesDone();
  EXPECT_FALSE(r->Read(&response_));
  auto status = r->Finish();
  EXPECT_THAT(status.error_message(), HasSubstr("Request is not supported yet"));
  EXPECT_FALSE(status.ok());
}

// We didn't implement cancellations yet, ensure we indicate so.
TEST_P(ServiceTest, CancelNotSupported) {
  request_ = nighthawk::client::ExecutionRequest();
  request_.mutable_cancellation_request();
  auto r = stub_->ExecutionStream(&context_);
  r->Write(request_, {});
  r->WritesDone();
  EXPECT_FALSE(r->Read(&response_));
  auto status = r->Finish();
  EXPECT_THAT(status.error_message(), HasSubstr("Request is not supported yet"));
  EXPECT_FALSE(status.ok());
}

TEST_P(ServiceTest, Unresolvable) {
  auto options = request_.mutable_start_request()->mutable_options();
  options->mutable_uri()->set_value("http://unresolvable-host/");
  runWithFailingValidationExpectations("Unknown failure");
}

} // namespace Client
} // namespace Nighthawk
