#include "client/output_transform_main.h"

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"

#include "api/client/service.pb.h"

#include "common/utility.h"

#include "client/factories_impl.h"
#include "client/options_impl.h"

#include "absl/strings/strip.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace Client {

OutputTransformMain::OutputTransformMain(int argc, const char* const* argv, std::istream& input)
    : input_(input) {
  const char* descr = "L7 (HTTP/HTTPS/HTTP2) performance characterization transformation tool.";
  TCLAP::CmdLine cmd(descr, ' ', "PoC"); // NOLINT

  std::vector<std::string> output_formats = {"human", "yaml", "json"};
  TCLAP::ValuesConstraint<std::string> output_formats_allowed(output_formats);
  output_format_ = "";
  TCLAP::ValueArg<std::string> output_format(
      "", "output-format",
      fmt::format("Verbosity of the output. Possible values: [human, yaml, json]."), true, "",
      &output_formats_allowed, cmd);
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
  try {
    if (!nighthawk::client::OutputFormat_OutputFormatOptions_Parse(
            absl::AsciiStrToUpper(output_format_), &translated_format)) {
      throw MalformedArgvException("Invalid output format");
    }
    std::string input = readInput();
    Envoy::MessageUtil::loadFromJson(input, output,
                                     Envoy::ProtobufMessage::getNullValidationVisitor());
  } catch (Envoy::EnvoyException e) {
    ENVOY_LOG(error, "Error: ", e.what());
    return 1;
  }

  // We override the output format, which will make the OutputCollectorFactory hand us
  // an instance that will output the desired format for us.
  output.mutable_options()->mutable_output_format()->set_value(translated_format);
  OutputCollectorPtr collector;
  try {
    const auto options = OptionsImpl(output.options());
    OutputCollectorFactoryImpl factory(time_system_, options);
    collector = factory.create();
  } catch (NighthawkException e) {
    ENVOY_LOG(error, "Error: ", e.what());
    return 2;
  }
  collector->setOutput(output);
  std::cout << collector->toString();
  return 0;
}

} // namespace Client
} // namespace Nighthawk
