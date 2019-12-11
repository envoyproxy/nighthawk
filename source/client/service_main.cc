#include "client/service_main.h"

#include "nighthawk/common/exception.h"
#include "nighthawk/common/version.h"

#include "common/utility.h"

#include "client/service_impl.h"

#include "absl/strings/strip.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace Client {

ServiceMain::ServiceMain(int argc, const char** argv) {
  const char* descr = "L7 (HTTP/HTTPS/HTTP2) performance characterization tool.";
  TCLAP::CmdLine cmd(descr, ' ', VersionUtils::VersionString()); // NOLINT

  TCLAP::ValueArg<std::string> listen_arg(
      "", "listen",
      "The address:port on which the Nighthawk gRPC service should listen. Default: "
      "0.0.0.0:8443.",
      false, "0.0.0.0:8443", "address:port", cmd);
  Utility::parseCommand(cmd, argc, argv);

  listener_bound_address_ = appendDefaultPortIfNeeded(listen_arg.getValue());
  ENVOY_LOG(info, "Nighthawk grpc service listener binding to: {}", listener_bound_address_);
  builder_.AddListeningPort(listener_bound_address_, grpc::InsecureServerCredentials(),
                            &listener_port_);
  builder_.RegisterService(&service_);
}

std::string ServiceMain::appendDefaultPortIfNeeded(absl::string_view host_and_maybe_port) {
  const size_t colon_index = Utility::findPortSeparator(host_and_maybe_port);
  std::string listener_address = std::string(host_and_maybe_port);
  if (colon_index == absl::string_view::npos) {
    listener_address += ":8443";
  }
  return listener_address;
}

void ServiceMain::start() {
  server_ = builder_.BuildAndStart();
  if (server_ == nullptr) {
    throw NighthawkException("Could not start the grpc service.");
  }
  if (absl::EndsWith(listener_bound_address_, ":0")) {
    listener_bound_address_ = std::string(absl::StripSuffix(listener_bound_address_, "0"));
    absl::StrAppend(&listener_bound_address_, listener_port_);
  }
  ENVOY_LOG(info, "Nighthawk grpc service listening on: {}", listener_bound_address_);
  channel_ = grpc::CreateChannel(listener_bound_address_, grpc::InsecureChannelCredentials());
  stub_ = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel_);
}

void ServiceMain::wait() { server_->Wait(); }

void ServiceMain::shutdown() { server_->Shutdown(); }

} // namespace Client
} // namespace Nighthawk
