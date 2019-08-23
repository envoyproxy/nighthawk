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
    parseIpAndMaybePort(internet_address_and_port.getValue());
    ENVOY_LOG(info, "Nighthawk grpc service listener binding to: {}", getListenerAddress());
    builder_.AddListeningPort(getListenerAddress(), grpc::InsecureServerCredentials(), &port_);
    builder_.RegisterService(&service_);
  } catch (Envoy::EnvoyException e) {
    throw NighthawkException(e.what());
  }
}

// TODO(oschaaf): move to utility class.
void ServiceMain::parseIpAndMaybePort(absl::string_view ip_and_maybe_port) {
  const size_t colon_index = Utility::findPortSeparator(ip_and_maybe_port);

  if (colon_index == absl::string_view::npos) {
    ip_ = std::string(ip_and_maybe_port);
  } else {
    ip_ = std::string(ip_and_maybe_port.substr(0, colon_index));
    port_ = std::stoi(std::string(ip_and_maybe_port.substr(colon_index + 1)));
  }
  if (ip_.size() > 1 && ip_[0] == '[' && ip_[ip_.length() - 1] == ']') {
    ip_ = std::string(ip_.substr(1, ip_.length() - 2));
  }
}

std::string ServiceMain::getListenerAddress() const {
  if (ip_.find(":") != std::string::npos) {
    return fmt::format("[{}]:{}", ip_, port_);
  } else {
    return fmt::format("{}:{}", ip_, port_);
  }
}

void ServiceMain::start() {
  server_ = builder_.BuildAndStart();
  if (server_ == nullptr) {
    throw NighthawkException("Could not start the grpc service.");
  }
  ENVOY_LOG(info, "Nighthawk grpc service listening on: {}", getListenerAddress());
  channel_ = grpc::CreateChannel(getListenerAddress(), grpc::InsecureChannelCredentials());
  stub_ = std::make_unique<nighthawk::client::NighthawkService::Stub>(channel_);
}

void ServiceMain::wait() { server_->Wait(); }

void ServiceMain::shutdown() { server_->Shutdown(); }

} // namespace Client
} // namespace Nighthawk
