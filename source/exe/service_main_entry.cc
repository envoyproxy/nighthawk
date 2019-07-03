#include <grpc++/grpc++.h>

#include <chrono>

#include "nighthawk/common/exception.h"

#include "common/utility.h"

#include "client/service_impl.h"

#include "absl/debugging/symbolize.h"
#include "api/client/service.pb.h"
#include "tclap/CmdLine.h"

// NOLINT(namespace-nighthawk)

namespace Nighthawk {
namespace Client {

class ServiceMain {
public:
  ServiceMain(int argc, char** argv) {
    const char* descr = "L7 (HTTP/HTTPS/HTTP2) performance characterization tool.";
    TCLAP::CmdLine cmd(descr, ' ', "PoC"); // NOLINT

    TCLAP::ValueArg<std::string> internet_address_and_port(
        "", "listen",
        "The address:port on which the Nighthawk grpc service should listen. Default: "
        "0.0.0.0:8443.",
        false, "0.0.0.0:8443", "uint32_t", cmd);

    Utility::parseCommand(cmd, argc, argv);

    try {
      listener_address_ = Envoy::Network::Utility::parseInternetAddressAndPort(
          internet_address_and_port.getValue());
    } catch (Envoy::EnvoyException e) {
      throw NighthawkException(e.what());
    }
  }

  void Run() {
    grpc::ServerBuilder builder;
    int grpc_server_port = listener_address_->ip()->port();
    builder.AddListeningPort(listener_address_->asString(), grpc::InsecureServerCredentials(),
                             &grpc_server_port);
    builder.RegisterService(&service_);
    server_ = builder.BuildAndStart();
    if (server_ == nullptr) {
      throw NighthawkException("Could not start the grpc service.");
    }
    std::cout << "Nighthawk grpc service listening: " << listener_address_->asString() << std::endl;
    channel_ =
        grpc::CreateChannel(listener_address_->asString(), grpc::InsecureChannelCredentials());
    stub_ = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel_);
    server_->Wait();
  }

  void Shutdown() { server_->Shutdown(); }

  Envoy::Network::Address::InstanceConstSharedPtr listener_address_;
  ServiceImpl service_;
  std::unique_ptr<grpc::Server> server_;
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<nighthawk::client::NighthawkService::Stub> stub_;
};

} // namespace Client
} // namespace Nighthawk

using namespace Nighthawk::Client;

int main(int argc, char** argv) {
#ifndef __APPLE__
  // absl::Symbolize mostly works without this, but this improves corner case
  // handling, such as running in a chroot jail.
  absl::InitializeSymbolizer(argv[0]);
#endif
  try {
    ServiceMain service(argc, argv); // NOLINT
    service.Run();
    service.Shutdown();
  } catch (const Nighthawk::Client::NoServingException& e) {
    return EXIT_SUCCESS;
  } catch (const Nighthawk::Client::MalformedArgvException& e) {
    return EXIT_FAILURE;
  } catch (const Nighthawk::NighthawkException& e) {
    std::cerr << "Failure: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return 0;
}
