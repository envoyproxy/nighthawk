#include "source/client/output_transform_main.h"

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"

#include "api/client/service.pb.h"

#include "source/client/factories_impl.h"
#include "source/client/options_impl.h"
#include "source/client/output_collector_impl.h"
#include "source/client/output_formatter_impl.h"
#include "source/common/utility.h"
#include "source/common/version_info.h"

#include "absl/strings/strip.h"
#include "fmt/ranges.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace Client {

OutputTransformMain::OutputTransformMain(int argc, const char* const* argv, std::istream& input)
    : input_(input) {
  const char* descr = "L7 (HTTP/HTTPS/HTTP2) performance characterization transformation tool.";
  TCLAP::CmdLine cmd(descr, ' ', VersionInfo::version()); // NOLINT
  std::vector<std::string> output_formats = OutputFormatterImpl::getLowerCaseOutputFormats();
  TCLAP::ValuesConstraint<std::string> output_formats_allowed(output_formats);
  TCLAP::ValueArg<std::string> output_format(
      "", "output-format", fmt::format("Output format. Possible values: {}.", output_formats), true,
      "", &output_formats_allowed, cmd);
  Utility::parseCommand(cmd, argc, argv);
  output_format_ = output_format.getValue();
}

std::string OutputTransformMain::readInput() {
  std::stringstream input;
  input << input_.rdbuf();
  return input.str();
}

uint32_t OutputTransformMain::run() {
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
  absl::StatusOr<std::string> format_status = formatter->formatProto(output);
  if (!format_status.ok()) {
    ENVOY_LOG(error, "error while formatting proto");
    return 1;
  }
  std::cout << *format_status;
  return 0;
}

} // namespace Client
} // namespace Nighthawk
