#include "client/service_main.h"

#include <grpc++/grpc++.h>

#include <chrono>

#include "nighthawk/common/exception.h"

#include "common/utility.h"

#include "client/service_impl.h"

#include "api/client/service.pb.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace Client {

ServiceMain::ServiceMain(int argc, const char** argv) {
  const char* descr = "L7 (HTTP/HTTPS/HTTP2) performance characterization tool.";
  TCLAP::CmdLine cmd(descr, ' ', "PoC"); // NOLINT

  TCLAP::ValueArg<std::string> internet_address_and_port(
      "", "listen",
      "The address:port on which the Nighthawk grpc service should listen. Default: "
      "0.0.0.0:8443.",
      false, "0.0.0.0:8443", "uint32_t", cmd);

  Utility::parseCommand(cmd, argc, argv);

  try {
    listener_address_ =
        Envoy::Network::Utility::parseInternetAddressAndPort(internet_address_and_port.getValue());
  } catch (Envoy::EnvoyException e) {
    throw NighthawkException(e.what());
  }
}

void ServiceMain::Run() {
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
  channel_ = grpc::CreateChannel(listener_address_->asString(), grpc::InsecureChannelCredentials());
  stub_ = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel_);
  server_->Wait();
}

void ServiceMain::Shutdown() { server_->Shutdown(); }

} // namespace Client
} // namespace Nighthawk
