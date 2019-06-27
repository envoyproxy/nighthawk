#include <grpc++/grpc++.h>

#include <chrono>

#include "nighthawk/common/exception.h"

#include "client/service_impl.h"

#include "test/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/environment.h"
#include "test/test_common/network_utility.h"

#include "api/client/service.pb.h"
#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;
using testing::MatchesRegex;

namespace Nighthawk {
namespace Client {

class ServiceTest : public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  void SetUp() override {
    grpc::ServerBuilder builder;
    int grpc_server_port = 0;
    const std::string loopback_address =
        Envoy::Network::Test::getLoopbackAddressUrlString(GetParam());

    builder.AddListeningPort(fmt::format("{}:0", loopback_address),
                             grpc::InsecureServerCredentials(), &grpc_server_port);
    builder.RegisterService(&service_);
    server_ = builder.BuildAndStart();
    channel_ = grpc::CreateChannel(fmt::format("{}:{}", loopback_address, grpc_server_port),
                                   grpc::InsecureChannelCredentials());
    stub_ = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel_);
    setBasicRequestOptions();
  }

  void TearDown() override { server_->Shutdown(); }

  void setBasicRequestOptions() {
    auto options = request_.mutable_start_request()->mutable_options();
    // XXX(oschaaf): Set defaults in the options header.
    // See if we can get rid of the ones in TCLAP to disambiguate
    // how we handle default values.
    options->mutable_uri()->set_value("http://127.0.0.1:10001/");
    options->mutable_duration()->set_seconds(3);
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
  auto r = stub_->ExecutionStream(&context_);
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
