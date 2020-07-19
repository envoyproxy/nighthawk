#include "adaptive_load/adaptive_load_client_main.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "nighthawk/adaptive_load/adaptive_load_controller.h"
#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/grpc/google_grpc_utils.h"
#include "external/envoy/source/common/protobuf/protobuf.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/client/service.grpc.pb.h"
#include "api/client/service.pb.h"

#include "common/utility.h"
#include "common/version_info.h"

#include "fmt/ranges.h"
#include "google/rpc/status.pb.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace AdaptiveLoad {

AdaptiveLoadMain::AdaptiveLoadMain(int argc, const char* const* argv,
                                   Envoy::TimeSource* time_source) {
  const char* descr =
      "Adaptive Load Controller tool that finds optimal load by sending a series of requests to "
      "a Nighthawk Service.";

  TCLAP::CmdLine cmd(descr, ' ', VersionInfo::version()); // NOLINT

  TCLAP::ValueArg<std::string> nighthawk_service_address("", "nighthawk-service-address",
                                                         "host:port for Nighthawk Service.", false,
                                                         "localhost:8443", "string", cmd);
  TCLAP::ValueArg<std::string> spec_filename(
      "", "spec-file",
      "Path to a textproto file describing the adaptive load session "
      "(nighthawk::adaptive_load::AdaptiveLoadSessionSpec).",
      true, "", "string", cmd);
  TCLAP::ValueArg<std::string> output_filename(
      "", "output-file",
      "Path to write adaptive load session output textproto "
      "(nighthawk::adaptive_load::AdaptiveLoadSessionOutput).",
      true, "", "string", cmd);

  Nighthawk::Utility::parseCommand(cmd, argc, argv);

  nighthawk_service_address_ = nighthawk_service_address.getValue();
  spec_filename_ = spec_filename.getValue();
  output_filename_ = output_filename.getValue();
  time_source_ = time_source;
}

uint32_t AdaptiveLoadMain::run() {
  std::ifstream ifs(spec_filename_);
  std::string spec_textproto((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());

  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  if (!Envoy::Protobuf::TextFormat::ParseFromString(spec_textproto, &spec)) {
    throw Envoy::EnvoyException("Unable to parse file \"" + spec_filename_ +
                                "\" as a text protobuf (type " + spec.GetTypeName() + ")");
  }

  std::shared_ptr<::grpc_impl::Channel> channel =
      ::grpc::CreateChannel(nighthawk_service_address_, ::grpc::InsecureChannelCredentials());

  std::unique_ptr<nighthawk::client::NighthawkService::StubInterface> stub(
      nighthawk::client::NighthawkService::NewStub(channel));

  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(stub.get(), spec, std::cerr, time_source_);

  std::ofstream ofs(output_filename_);
  if (ofs.is_open()) {
    ofs << output.DebugString();
  } else {
    throw Envoy::EnvoyException("Unable to open output file \"" + output_filename_ + "\"");
  }
  if (output.session_status().code() != 0) {
    std::cerr << output.session_status().message() << "\n";
  }
  return 0;
}

} // namespace AdaptiveLoad
} // namespace Nighthawk
