#include <chrono>

#include "nighthawk/common/exception.h"

#include "test/mocks.h"

#include "gtest/gtest.h"

#include "test/mocks/upstream/mocks.h"

#include <grpc++/grpc++.h>

#include "test/test_common/environment.h"
#include "test/test_common/network_utility.h"

#include "api/client/service.pb.h"
#include "client/service_impl.h"
#include "common/grpc/async_client_impl.h"

using namespace std::chrono_literals;
using namespace testing;

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
  }

  void TearDown() override { server_->Shutdown(); }

  ServiceImpl service_;
  std::unique_ptr<grpc::Server> server_;
  std::shared_ptr<grpc::Channel> channel_;
  grpc::ClientContext context_;
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ServiceTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(ServiceTest, Basic) {
  nighthawk::client::NighthawkService::Stub stub(channel_);
  nighthawk::client::SendCommandRequest request;
  nighthawk::client::SendCommandResponse response;

  auto r = stub.SendCommand(&context_);
  auto options = request.mutable_options();
  options->set_uri("http://127.0.0.1:10001/");
  options->set_connections(1);
  options->set_concurrency("1");
  options->mutable_duration()->set_seconds(3);
  options->set_output_format("human");
  options->set_requests_per_second(3);
  options->mutable_request_options()->set_request_method(envoy::api::v2::core::RequestMethod::GET);
  options->set_address_family("v4");

  request.set_command_type(
      nighthawk::client::SendCommandRequest_CommandType::SendCommandRequest_CommandType_kStart);
  r->Write(request, {});
  // request.set_command_type(
  //    nighthawk::client::SendCommandRequest_CommandType::SendCommandRequest_CommandType_kUpdate);
  // r->Write(request, {});
  r->WritesDone();
  EXPECT_TRUE(r->Read(&response));

  // std::cerr << response.DebugString() << std::endl;
  auto status = r->Finish();
  EXPECT_TRUE(status.ok());
}

TEST_P(ServiceTest, AttemptDoubleStart) {
  nighthawk::client::NighthawkService::Stub stub(channel_);
  nighthawk::client::SendCommandRequest request;
  nighthawk::client::SendCommandResponse response;

  auto r = stub.SendCommand(&context_);
  auto options = request.mutable_options();
  options->set_uri("http://127.0.0.1:10001/");
  options->set_connections(1);
  options->set_concurrency("1");
  options->mutable_duration()->set_seconds(3);
  options->set_output_format("human");
  options->set_requests_per_second(3);
  options->mutable_request_options()->set_request_method(envoy::api::v2::core::RequestMethod::GET);
  options->set_address_family("v4");

  request.set_command_type(
      nighthawk::client::SendCommandRequest_CommandType::SendCommandRequest_CommandType_kStart);
  EXPECT_TRUE(r->Write(request, {}));
  request.set_command_type(
      nighthawk::client::SendCommandRequest_CommandType::SendCommandRequest_CommandType_kStart);
  EXPECT_TRUE(r->Write(request, {}));
  EXPECT_TRUE(r->WritesDone());
  EXPECT_TRUE(r->Read(&response));
  // std::cerr << response.DebugString() << std::endl;
  auto status = r->Finish();
  EXPECT_FALSE(status.ok());
}

} // namespace Client
} // namespace Nighthawk
