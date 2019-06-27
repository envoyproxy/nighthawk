#include <grpc++/grpc++.h>

#include <chrono>

#include "nighthawk/common/exception.h"

#include "client/service_impl.h"

#include "api/client/service.pb.h"

#include "absl/debugging/symbolize.h"

// NOLINT(namespace-nighthawk)

namespace Nighthawk {
namespace Client {

class ServiceMain {
public:
  ServiceMain() {
    grpc::ServerBuilder builder;
    int grpc_server_port = 0;
    const std::string loopback_address = "127.0.0.1";
    //    Envoy::Network::Test::getLoopbackAddressUrlString(GetParam());

    builder.AddListeningPort(fmt::format("{}:0", loopback_address),
                             grpc::InsecureServerCredentials(), &grpc_server_port);
    builder.RegisterService(&service_);
    server_ = builder.BuildAndStart();
    channel_ = grpc::CreateChannel(fmt::format("{}:{}", loopback_address, grpc_server_port),
                                   grpc::InsecureChannelCredentials());
    stub_ = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel_);
  }

  void Shutdown() { server_->Shutdown(); }

  ServiceImpl service_;
  std::unique_ptr<grpc::Server> server_;
  std::shared_ptr<grpc::Channel> channel_;
  grpc::ClientContext context_;
  nighthawk::client::ExecutionRequest request_;
  nighthawk::client::ExecutionResponse response_;
  std::unique_ptr<nighthawk::client::NighthawkService::Stub> stub_;
};

} // namespace Client
} // namespace Nighthawk

using namespace Nighthawk::Client;

int main(int, char** argv) {
#ifndef __APPLE__
  // absl::Symbolize mostly works without this, but this improves corner case
  // handling, such as running in a chroot jail.
  absl::InitializeSymbolizer(argv[0]);
#endif
  try {
    ServiceMain service;
    std::cout << "Server listening" << std::endl;
    service.server_->Wait();
    service.Shutdown();
  } catch (const Nighthawk::Client::NoServingException& e) {
    return EXIT_SUCCESS;
  } catch (const Nighthawk::Client::MalformedArgvException& e) {
    return EXIT_FAILURE;
  } catch (const Nighthawk::NighthawkException& e) {
    return EXIT_FAILURE;
  }
  return 0;
}
