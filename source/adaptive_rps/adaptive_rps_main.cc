#include "adaptive_rps/adaptive_rps_main.h"

#include "nighthawk/common/exception.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "api/client/service.pb.h"
#include "common/utility.h"
#include "common/version_info.h"
#include "absl/strings/strip.h"
#include "fmt/ranges.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace AdaptiveRps {

AdaptiveRpsMain::AdaptiveRpsMain(int argc, const char* const* argv) {
  const char* descr = "Adaptive RPS tool that finds optimal RPS by sending a series of requests to "
                      "a Nighthawk Service.";

  TCLAP::CmdLine cmd(descr, ' ', VersionInfo::version()); // NOLINT

  TCLAP::ValueArg<std::string> api_server("localhost:8443", "api-server", "host:port for Nighthawk Service."), true, cmd);
  TCLAP::ValueArg<std::string> spec_filename("", "spec-file", "Path to a textproto file describing the adaptive RPS session (nighthawk::adaptive_rps::AdaptiveRpsSessionSpec)."), true, cmd);
  TCLAP::ValueArg<std::string> output_filename("", "output-file", "Path to write adaptive RPS session output textproto (nighthawk::adaptive_rps::AdaptiveRpsSessionOutput)."), true, cmd);

  Utility::parseCommand(cmd, argc, argv);

  api_server_ = api_server.getValue();
  spec_filename_ = spec_filename.getValue();
  output_filename_ = output_filename.getValue();
}

uint32_t AdaptiveRpsMain::run() {
  // Figure out the desired output format, and read attempt to read the input proto
  // from stdin.
  nighthawk::client::OutputFormat_OutputFormatOptions translated_format;
  nighthawk::client::Output output;
  RELEASE_ASSERT(nighthawk::client::OutputFormat_OutputFormatOptions_Parse(
                     absl::AsciiStrToUpper(output_format_), &translated_format),
                 "Invalid output format");
  std::string input = readInput();
  try {
    Envoy::MessageUtil::loadFromJson(input, output,
                                     Envoy::ProtobufMessage::getStrictValidationVisitor());
  } catch (Envoy::EnvoyException& e) {
    std::cerr << "Input error: " << e.what();
    return 1;
  }
  OutputFormatterFactoryImpl factory;
  OutputFormatterPtr formatter = factory.create(translated_format);
  std::cout << formatter->formatProto(output);
  return 0;
}

} // namespace AdaptiveRps
} // namespace Nighthawk
