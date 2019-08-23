#include "client/service_main.h"

#include "nighthawk/common/exception.h"

#include "common/utility.h"

#include "client/service_impl.h"

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
      false, "0.0.0.0:8443", "address:port", cmd);

  Utility::parseCommand(cmd, argc, argv);

  try {
    listener_address_ =
        Envoy::Network::Utility::parseInternetAddressAndPort(internet_address_and_port.getValue());
  } catch (Envoy::EnvoyException e) {
    throw NighthawkException(e.what());
  }
}

void ServiceMain::start() {
  grpc::ServerBuilder builder;
  int grpc_server_port = listener_address_->ip()->port();
  builder.AddListeningPort(listener_address_->asString(), grpc::InsecureServerCredentials(),
                           &grpc_server_port);
  builder.RegisterService(&service_);
  server_ = builder.BuildAndStart();
  if (server_ == nullptr) {
    throw NighthawkException("Could not start the grpc service.");
  }

  if (!listener_address_->ip()->port()) {
    ASSERT(grpc_server_port != 0);
    listener_address_ = Envoy::Network::Utility::parseInternetAddressAndPort(
        fmt::format("{}:{}", listener_address_->ip()->addressAsString(), grpc_server_port));
  } else {
    ASSERT(listener_address_->ip()->port() == static_cast<uint32_t>(grpc_server_port));
  }
  ENVOY_LOG(info, "Nighthawk grpc service listening: {}", listener_address_->asString());
  channel_ = grpc::CreateChannel(listener_address_->asString(), grpc::InsecureChannelCredentials());
  stub_ = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel_);
}

void ServiceMain::wait() { server_->Wait(); }

void ServiceMain::shutdown() { server_->Shutdown(); }

} // namespace Client
} // namespace Nighthawk
