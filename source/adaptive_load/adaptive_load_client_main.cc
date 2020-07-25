#include "adaptive_load/adaptive_load_client_main.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

//  "third_party/envoy/src/include/envoy/filesystem/filesystem.h"
// #include "envoy/filesystem/filesystem.h"
#include "envoy/common/exception.h"
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

namespace {

/**
 * Writes a string to a file.
 *
 * @throw Nighthawk::NighthawkException For any filesystem error.
 */
void WriteFileOrThrow(Envoy::Filesystem::Instance& filesystem, std::string& path,
                      std::string& contents) {
  Envoy::Filesystem::FilePtr out_file = filesystem.createFile(path);
  const Envoy::Api::IoCallBoolResult open_result =
      out_file->open(1 << Envoy::Filesystem::File::Operation::Write);
  if (!open_result.ok()) {
    throw Nighthawk::NighthawkException("Unable to open output file \"" + path +
                                        "\": " + open_result.err_->getErrorDetails());
  }
  const Envoy::Api::IoCallSizeResult write_result = out_file->write(contents);
  if (!write_result.ok()) {
    throw Nighthawk::NighthawkException("Unable to write output file \"" + path +
                                        "\": " + write_result.err_->getErrorDetails());
  }
  const Envoy::Api::IoCallBoolResult close_result = out_file->close();
  if (!close_result.ok()) {
    throw Nighthawk::NighthawkException("Unable to close output file \"" + path +
                                        "\": " + close_result.err_->getErrorDetails());
  }
}

} // namespace

AdaptiveLoadClientMain::AdaptiveLoadClientMain(int argc, const char* const* argv,
                                               Envoy::Filesystem::Instance& filesystem,
                                               Envoy::TimeSource& time_source)
    : filesystem_{filesystem}, time_source_{time_source} {
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
}

uint32_t AdaptiveLoadClientMain::run() {
  std::string spec_textproto;
  try {
    spec_textproto = filesystem_.fileReadToEnd(spec_filename_);
  } catch (const Envoy::EnvoyException& e) {
    throw Nighthawk::NighthawkException("Failed to read spec textproto file \"" + spec_filename_ +
                                        "\": " + e.what());
  }

  nighthawk::adaptive_load::AdaptiveLoadSessionSpec spec;
  if (!Envoy::Protobuf::TextFormat::ParseFromString(spec_textproto, &spec)) {
    throw Nighthawk::NighthawkException("Unable to parse file \"" + spec_filename_ +
                                        "\" as a text protobuf (type " + spec.GetTypeName() + ")");
  }

  std::shared_ptr<::grpc_impl::Channel> channel =
      ::grpc::CreateChannel(nighthawk_service_address_, ::grpc::InsecureChannelCredentials());

  std::unique_ptr<nighthawk::client::NighthawkService::StubInterface> stub(
      nighthawk::client::NighthawkService::NewStub(channel));

  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output =
      PerformAdaptiveLoadSession(stub.get(), spec, time_source_);

  std::string output_textproto = output.DebugString();

  WriteFileOrThrow(filesystem_, output_filename_, output_textproto);

  if (output.session_status().code() != 0) {
    std::cerr << output.session_status().message() << "\n";
  }
  return 0;
}

} // namespace Nighthawk
