#include "client/service_main.h"

#include <fstream>
#include <iostream>

#include "nighthawk/common/exception.h"

#include "common/utility.h"

#include "client/service_impl.h"

#include "absl/strings/strip.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace Client {

namespace {
std::function<void(int)> signal_handler_delegate;
void signal_handler(int signal) { signal_handler_delegate(signal); }
} // namespace

ServiceMain::ServiceMain(int argc, const char** argv) {
  const char* descr = "L7 (HTTP/HTTPS/HTTP2) performance characterization tool.";
  TCLAP::CmdLine cmd(descr, ' ', "PoC"); // NOLINT

  TCLAP::ValueArg<std::string> listen_arg(
      "", "listen",
      "The address:port on which the Nighthawk grpc service should listen. Default: "
      "0.0.0.0:8443.",
      false, "0.0.0.0:8443", "address:port", cmd);

  TCLAP::ValueArg<std::string> address_file_arg(
      "", "listener-address-file",
      "Location where the service will write the final address:port on which the Nighthawk grpc "
      "service listens. Default empty.",
      false, "", "", cmd);

  Utility::parseCommand(cmd, argc, argv);

  listener_bound_address_ = appendDefaultPortIfNeeded(listen_arg.getValue());
  if (address_file_arg.isSet()) {
    listener_output_path_ = address_file_arg.getValue();
  }
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
  if (listener_output_path_ != "") {
    std::ofstream myfile(listener_output_path_);
    if (myfile.is_open()) {
      myfile << listener_bound_address_;
      myfile.close();
    }
  }
  channel_ = grpc::CreateChannel(listener_bound_address_, grpc::InsecureChannelCredentials());
  stub_ = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel_);
}

void ServiceMain::wait() {
  signal_handler_delegate = [&](int) { shutdown(); };
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  server_->Wait();
}

void ServiceMain::shutdown() {
  ENVOY_LOG(info, "Nighthawk grpc service shutting down");
  server_->Shutdown();
}

} // namespace Client
} // namespace Nighthawk
