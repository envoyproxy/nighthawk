#include "adaptive_load/adaptive_load_client_main.h"

#include <cstring>
#include <memory>
#include <string>
#include <utility>

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
 * @param filesystem Envoy abstraction around filesystem functions, to facilitate unit testing.
 * @param path Relative or absolute path to the file to write.
 * @param contents String to write to the file.
 *
 * @throw Nighthawk::NighthawkException For any filesystem error.
 */
void WriteFileOrThrow(Envoy::Filesystem::Instance& filesystem, absl::string_view path,
                      absl::string_view contents) {
  Envoy::Filesystem::FilePtr file = filesystem.createFile(std::string(path));
  const Envoy::Api::IoCallBoolResult open_result =
      file->open(((1 << Envoy::Filesystem::File::Operation::Write)) |
                 (1 << (Envoy::Filesystem::File::Operation::Create)));
  if (!open_result.ok()) {
    throw Nighthawk::NighthawkException(absl::StrCat("Unable to open output file \"", path,
                                                     "\": ", open_result.err_->getErrorDetails()));
  }
  const Envoy::Api::IoCallSizeResult write_result = file->write(contents);
  if (!write_result.ok()) {
    throw Nighthawk::NighthawkException(absl::StrCat("Unable to write to output file \"", path,
                                                     "\": ", write_result.err_->getErrorDetails()));
  }
  const Envoy::Api::IoCallBoolResult close_result = file->close();
  if (!close_result.ok()) {
    throw Nighthawk::NighthawkException(absl::StrCat("Unable to close output file \"", path,
                                                     "\": ", close_result.err_->getErrorDetails()));
  }
}

} // namespace

AdaptiveLoadClientMain::AdaptiveLoadClientMain(int argc, const char* const* argv,
                                               AdaptiveLoadController& controller,
                                               Envoy::Filesystem::Instance& filesystem)
    : controller_{controller}, filesystem_{filesystem} {
  TCLAP::CmdLine cmd("Adaptive Load tool that finds the optimal load on the target " // NOLINT
                     "through a series of Nighthawk Service benchmarks.",
                     /*delimiter=*/' ', VersionInfo::version());

  TCLAP::ValueArg<std::string> nighthawk_service_address(
      /*flag=*/"", "nighthawk-service-address",
      "host:port for Nighthawk Service. To enable TLS, set --use-tls.",
      /*req=*/false, "localhost:8443", "string", cmd);
  TCLAP::SwitchArg use_tls(
      /*flag=*/"", "use-tls",
      "Use TLS for the gRPC connection from this program to the Nighthawk Service. Set environment "
      "variable GRPC_DEFAULT_SSL_ROOTS_FILE_PATH to override the default root certificates.",
      cmd);
  TCLAP::ValueArg<std::string> spec_filename(
      /*flag=*/"", "spec-file",
      "Path to a textproto file describing the adaptive load session "
      "(nighthawk::adaptive_load::AdaptiveLoadSessionSpec).",
      /*req=*/true, /*val=*/"", "string", cmd);
  TCLAP::ValueArg<std::string> output_filename(
      /*flag=*/"", "output-file",
      "Path to write adaptive load session output textproto "
      "(nighthawk::adaptive_load::AdaptiveLoadSessionOutput).",
      /*req=*/true, /*val=*/"", "string", cmd);

  Nighthawk::Utility::parseCommand(cmd, argc, argv);

  nighthawk_service_address_ = nighthawk_service_address.getValue();
  use_tls_ = use_tls.getValue();
  spec_filename_ = spec_filename.getValue();
  output_filename_ = output_filename.getValue();
}

uint32_t AdaptiveLoadClientMain::Run() {
  ENVOY_LOG(info, "Attempting adaptive load session: {}", DescribeInputs());
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
  std::shared_ptr<::grpc::Channel> channel = grpc::CreateChannel(
      nighthawk_service_address_, use_tls_ ? grpc::SslCredentials(grpc::SslCredentialsOptions())
                                           : grpc::InsecureChannelCredentials());
  std::unique_ptr<nighthawk::client::NighthawkService::StubInterface> stub(
      nighthawk::client::NighthawkService::NewStub(channel));

  absl::StatusOr<nighthawk::adaptive_load::AdaptiveLoadSessionOutput> output_or =
      controller_.PerformAdaptiveLoadSession(stub.get(), spec);
  if (!output_or.ok()) {
    ENVOY_LOG(error, "Error in adaptive load session: {}", output_or.status().message());
    return 1;
  }
  nighthawk::adaptive_load::AdaptiveLoadSessionOutput output = output_or.value();
  WriteFileOrThrow(filesystem_, output_filename_, output.DebugString());
  return 0;
}

std::string AdaptiveLoadClientMain::DescribeInputs() {
  return "Nighthawk Service " + nighthawk_service_address_ + " using " +
         (use_tls_ ? "TLS" : "insecure") + " connection, input file: " + spec_filename_ +
         ", output file: " + output_filename_;
}

} // namespace Nighthawk
