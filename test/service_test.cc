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
    options->set_uri("http://127.0.0.1:10001/");
    options->set_verbosity("info");
    options->set_connections(1);
    options->set_concurrency("1");
    options->mutable_duration()->set_seconds(3);
    options->set_output_format("human");
    options->set_requests_per_second(3);
    options->mutable_request_options()->set_request_method(
        envoy::api::v2::core::RequestMethod::GET);
    options->set_address_family("v4");
  }

  void runWithFailingValidationExpectations(absl::string_view match_error = "") {
    auto r = stub_->sendCommand(&context_);
    r->Write(request_, {});
    r->WritesDone();
    EXPECT_TRUE(r->Read(&response_));
    auto status = r->Finish();

    if (!match_error.empty()) {
      EXPECT_THAT(status.error_message(), HasSubstr(std::string(match_error)));
    }
    EXPECT_FALSE(status.ok());
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

TEST_P(ServiceTest, Basic) {
  auto r = stub_->sendCommand(&context_);
  r->Write(request_, {});
  r->WritesDone();
  EXPECT_TRUE(r->Read(&response_));
  EXPECT_EQ(6, response_.output().results(0).counters().size());
  auto status = r->Finish();
  EXPECT_TRUE(status.ok());
}

TEST_P(ServiceTest, NoConcurrentStart) {
  auto r = stub_->sendCommand(&context_);
  EXPECT_TRUE(r->Write(request_, {}));
  EXPECT_TRUE(r->Write(request_, {}));
  EXPECT_TRUE(r->WritesDone());
  EXPECT_TRUE(r->Read(&response_));
  auto status = r->Finish();
  EXPECT_FALSE(status.ok());
}

TEST_P(ServiceTest, BackToBackExecution) {
  auto r = stub_->sendCommand(&context_);
  EXPECT_TRUE(r->Write(request_, {}));
  EXPECT_TRUE(r->Read(&response_));
  EXPECT_TRUE(r->Write(request_, {}));
  EXPECT_TRUE(r->Read(&response_));
  EXPECT_TRUE(r->WritesDone());
  auto status = r->Finish();
  EXPECT_TRUE(status.ok());
}

TEST_P(ServiceTest, InvalidRps) {
  auto options = request_.mutable_start_request()->mutable_options();
  options->set_requests_per_second(0);
  runWithFailingValidationExpectations(
      "CommandLineOptionsValidationError.RequestsPerSecond: [\"value must be inside range");
}

TEST_P(ServiceTest, UpdatesNotSupported) {
  request_ = nighthawk::client::ExecutionRequest();
  request_.mutable_update_request();
  auto r = stub_->sendCommand(&context_);
  r->Write(request_, {});
  r->WritesDone();
  EXPECT_FALSE(r->Read(&response_));
  auto status = r->Finish();
  EXPECT_THAT(status.error_message(), HasSubstr("Configuration updates are not supported yet"));
  EXPECT_FALSE(status.ok());
}

} // namespace Client
} // namespace Nighthawk
