#include <grpc++/grpc++.h>

#include <chrono>

#include "nighthawk/common/exception.h"

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

    // TODO(oschaaf): duplicate code, shared with the CLI
    cmd.setExceptionHandling(false);
    try {
      cmd.parse(argc, argv);
    } catch (TCLAP::ArgException& e) {
      try {
        cmd.getOutput()->failure(cmd, e);
      } catch (const TCLAP::ExitException&) {
        // failure() has already written an informative message to stderr, so all that's left to do
        // is throw our own exception with the original message.
        throw MalformedArgvException(e.what());
      }
    } catch (const TCLAP::ExitException& e) {
      // parse() throws an ExitException with status 0 after printing the output for --help and
      // --version.
      throw NoServingException();
    }
    // end duplicate code, shared with the CLI. Fix this.

    auto listener_address =
        Envoy::Network::Utility::parseInternetAddressAndPort(internet_address_and_port.getValue());
    grpc::ServerBuilder builder;
    int grpc_server_port = 0;
    builder.AddListeningPort(listener_address->asString(), grpc::InsecureServerCredentials(),
                             &grpc_server_port);
    builder.RegisterService(&service_);
    server_ = builder.BuildAndStart();
    channel_ =
        grpc::CreateChannel(listener_address->asString(), grpc::InsecureChannelCredentials());
    stub_ = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel_);
  }

  void Shutdown() { server_->Shutdown(); }

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
    ServiceMain service(argc, argv);
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
