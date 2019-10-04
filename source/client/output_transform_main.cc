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

OutputTransformMain::OutputTransformMain(int argc, const char* const* argv) {
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
  while (getline(std::cin, line)) {
    input << line << std::endl;
  }
  return input.str();
}

uint32_t OutputTransformMain::run() {
  nighthawk::client::CommandLineOptions proto_options;
  nighthawk::client::OutputFormat_OutputFormatOptions translated_format;
  RELEASE_ASSERT(nighthawk::client::OutputFormat_OutputFormatOptions_Parse(
                     absl::AsciiStrToUpper(output_format_), &translated_format),
                 "Invalid output format");

  proto_options.mutable_output_format()->set_value(translated_format);
  // required, but not used in this CLI
  proto_options.mutable_uri()->set_value("http://foo");
  Nighthawk::Client::OptionsImpl options(proto_options);
  nighthawk::client::Output output;
  *output.mutable_options() = proto_options;
  try {
    std::string input = readInput();
    Envoy::MessageUtil::loadFromJson(input, output,
                                     Envoy::ProtobufMessage::getStrictValidationVisitor());
  } catch (Envoy::EnvoyException e) {
    throw MalformedArgvException(e.what());
  }
  OutputCollectorFactoryImpl factory(time_system_, options);
  auto collector = factory.create();
  collector->setOutput(output);
  std::cout << collector->toString();
  return 0;
}

} // namespace Client
} // namespace Nighthawk
