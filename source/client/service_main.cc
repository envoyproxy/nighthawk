#include "client/service_main.h"

#include <fstream>
#include <iostream>

#include "nighthawk/common/exception.h"

#include "common/utility.h"
#include "common/version_info.h"

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
  channel_ = grpc::CreateChannel(listener_bound_address_, grpc::InsecureChannelCredentials());
  stub_ = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel_);
  pipe_fds_.resize(2);
  // The shutdown thread will be notified of by our signal handler and take it from there.
  RELEASE_ASSERT(pipe(pipe_fds_.data()) == 0, "pipe failed");

  shutdown_thread_ = std::thread([this]() {
    int tmp;
    RELEASE_ASSERT(read(pipe_fds_[0], &tmp, sizeof(int)) >= 0, "read failed");
    RELEASE_ASSERT(close(pipe_fds_[0]) == 0, "read side close failed");
    RELEASE_ASSERT(close(pipe_fds_[1]) == 0, "write side close failed");
    pipe_fds_.clear();
    server_->Shutdown();
  });
}

void ServiceMain::wait() {
  signal_handler_delegate = [this](int) { onSignal(); };
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  server_->Wait();
  shutdown();
}

void ServiceMain::onSignal() { initiateShutdown(); }

void ServiceMain::initiateShutdown() {
  if (pipe_fds_.size() == 2) {
    const int tmp = 0;
    RELEASE_ASSERT(write(pipe_fds_[1], &tmp, sizeof(int)) == sizeof(int), "write failed");
  }
}

void ServiceMain::shutdown() {
  initiateShutdown();
  if (shutdown_thread_.joinable()) {
    shutdown_thread_.join();
  }
  ENVOY_LOG(info, "Nighthawk grpc service exits");
}

} // namespace Client
} // namespace Nighthawk
