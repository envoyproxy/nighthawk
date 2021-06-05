#include "source/client/service_main.h"

#include <fstream>
#include <iostream>

#include "nighthawk/common/exception.h"

#include "source/client/service_impl.h"
#include "source/common/utility.h"
#include "source/common/version_info.h"

#include "absl/strings/strip.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace Client {

ServiceMain::ServiceMain(int argc, const char** argv) {
  const char* descr = "L7 (HTTP/HTTPS/HTTP2) performance characterization tool.";
  TCLAP::CmdLine cmd(descr, ' ', VersionInfo::version()); // NOLINT

  TCLAP::ValueArg<std::string> listen_arg(
      "", "listen",
      "The address:port on which the Nighthawk gRPC service should listen. Default: "
      "0.0.0.0:8443.",
      false, "0.0.0.0:8443", "address:port", cmd);

  TCLAP::ValueArg<std::string> address_file_arg(
      "", "listener-address-file",
      "Location where the service will write the final address:port on which the Nighthawk grpc "
      "service listens. Default empty.",
      false, "", "", cmd);

  std::vector<std::string> service_names{"traffic-generator-service", "dummy-request-source"};
  TCLAP::ValuesConstraint<std::string> service_names_allowed(service_names);
  TCLAP::ValueArg<std::string> service_arg(
      "", "service", "Specifies which service to run. Default 'traffic-generator-service'.", false,
      "traffic-generator-service", &service_names_allowed, cmd);
  Utility::parseCommand(cmd, argc, argv);

  if (service_arg.getValue() == "traffic-generator-service") {
    service_ = std::make_unique<ServiceImpl>();
  } else if (service_arg.getValue() == "dummy-request-source") {
    service_ = std::make_unique<RequestSourceServiceImpl>();
  }
  RELEASE_ASSERT(service_ != nullptr, "Service mapping failed");
  listener_bound_address_ = appendDefaultPortIfNeeded(listen_arg.getValue());
  if (address_file_arg.isSet()) {
    listener_output_path_ = address_file_arg.getValue();
  }
  ENVOY_LOG(info, "Nighthawk grpc service listener binding to: {}", listener_bound_address_);
  builder_.AddListeningPort(listener_bound_address_, grpc::InsecureServerCredentials(),
                            &listener_port_);
  builder_.RegisterService(service_.get());
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
    }
  }
  signal_handler_ = std::make_unique<SignalHandler>([this]() { server_->Shutdown(); });
}

void ServiceMain::wait() {
  server_->Wait();
  shutdown();
}

void ServiceMain::shutdown() { ENVOY_LOG(info, "Nighthawk grpc service exits"); }

} // namespace Client
} // namespace Nighthawk
