#include "client/output_transform_main.h"

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"

#include "api/client/service.pb.h"

#include "common/utility.h"

#include "client/factories_impl.h"
#include "client/options_impl.h"
#include "client/output_collector_impl.h"

#include "absl/strings/strip.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace Client {

OutputTransformMain::OutputTransformMain(int argc, const char* const* argv, std::istream& input)
    : input_(input) {
  const char* descr = "L7 (HTTP/HTTPS/HTTP2) performance characterization transformation tool.";
  TCLAP::CmdLine cmd(descr, ' ', "PoC"); // NOLINT

  // TODO(oschaaf): create a canonicalize way to get the supported output formats.
  // De-duplicate the code here arg handling code with the nighthawk_client CLI
  std::vector<std::string> output_formats = {"human", "yaml", "json"};
  TCLAP::ValuesConstraint<std::string> output_formats_allowed(output_formats);
  output_format_ = "";
  TCLAP::ValueArg<std::string> output_format(
      "", "output-format", fmt::format("Output format. Possible values: [human, yaml, json]."),
      true, "", &output_formats_allowed, cmd);
  Utility::parseCommand(cmd, argc, argv);
  output_format_ = output_format.getValue();
}

std::string OutputTransformMain::readInput() {
  std::stringstream input;
  std::string line;
  while (getline(input_, line)) {
    input << line << std::endl;
  }
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
  formatter->setProto(output);
  std::cout << formatter->toString();
  return 0;
}

} // namespace Client
} // namespace Nighthawk
