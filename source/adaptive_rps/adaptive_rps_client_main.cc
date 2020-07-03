#include "adaptive_rps/adaptive_rps_client_main.h"

#include "adaptive_rps/adaptive_rps_controller.h"
#include "nighthawk/common/exception.h"
// #include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "api/adaptive_rps/adaptive_rps.pb.h"
#include "api/client/service.grpc.pb.h"
#include "api/client/service.pb.h"
#include "common/utility.h"
#include "common/version_info.h"
#include "fmt/ranges.h"
#include "tclap/CmdLine.h"

#include "common/grpc/google_grpc_utils.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

// #include "common/protobuf/protobuf.h"
// #include "google/protobuf/wrappers.pb.h"
#include "google/rpc/status.pb.h"
#include "nighthawk/common/exception.h"

#include "common/utility.h"
#include "common/version_info.h"

// #include "third_party/envoy/src/source/common/protobuf/message_validator_impl.h"
// #include "third_party/envoy/src/source/common/protobuf/protobuf.h"
// #include "third_party/envoy/src/source/common/protobuf/visitor.h"
// #include "third_party/envoy/src/source/common/protobuf/well_known.h"

// #include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/protobuf.h"
// #include "external/envoy/source/common/protobuf/utility.h"

namespace Nighthawk {
namespace AdaptiveRps {

AdaptiveRpsMain::AdaptiveRpsMain(int argc, const char* const* argv) {
  const char* descr = "Adaptive RPS tool that finds optimal RPS by sending a series of requests to "
                      "a Nighthawk Service.";

  TCLAP::CmdLine cmd(descr, ' ', VersionInfo::version()); // NOLINT

  TCLAP::ValueArg<std::string> nighthawk_service_address(
      "", "nighthawk-service-address", "host:port for Nighthawk Service.", false, "localhost:8443",
      "string", cmd);
  TCLAP::ValueArg<std::string> spec_filename(
      "", "spec-file",
      "Path to a textproto file describing the adaptive RPS session "
      "(nighthawk::adaptive_rps::AdaptiveRpsSessionSpec).",
      true, "", "string", cmd);
  TCLAP::ValueArg<std::string> output_filename(
      "", "output-file",
      "Path to write adaptive RPS session output textproto "
      "(nighthawk::adaptive_rps::AdaptiveRpsSessionOutput).",
      true, "", "string", cmd);

  Nighthawk::Utility::parseCommand(cmd, argc, argv);

  nighthawk_service_address_ = nighthawk_service_address.getValue();
  spec_filename_ = spec_filename.getValue();
  output_filename_ = output_filename.getValue();
}

uint32_t AdaptiveRpsMain::run() {
  std::ifstream ifs(spec_filename_);
  std::string spec_textproto((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());

  nighthawk::adaptive_rps::AdaptiveRpsSessionSpec spec;
  if (!Envoy::Protobuf::TextFormat::ParseFromString(spec_textproto, &spec)) {
    throw Envoy::EnvoyException("Unable to parse file \"" + spec_filename_ +
                                "\" as a text protobuf (type " + spec.GetTypeName() + ")");
  }

  std::shared_ptr<grpc_impl::Channel> channel =
      ::grpc::CreateChannel(nighthawk_service_address_, ::grpc::InsecureChannelCredentials());

  // std::shared_ptr<grpc_impl::ChannelCredentials> credentials;
  // credentials = grpc::experimental::LocalCredentials(LOCAL_TCP);

  // std::shared_ptr<grpc_impl::Channel> channel =
  //     grpc::CreateChannel(nighthawk_service_address_, credentials);

  std::unique_ptr<nighthawk::client::NighthawkService::Stub> stub(
      nighthawk::client::NighthawkService::NewStub(channel));

  nighthawk::adaptive_rps::AdaptiveRpsSessionOutput output =
      PerformAdaptiveRpsSession(stub.get(), spec);

  std::ofstream ofs(output_filename_);
  if (ofs.is_open()) {
    ofs << output.DebugString();
  } else {
    throw Envoy::EnvoyException("Unable to open output file \"" + output_filename_ + "\"");
  }
  return 0;
}

} // namespace AdaptiveRps
} // namespace Nighthawk
