#include "client/options_impl.h"

#include "common/protobuf/message_validator_impl.h"
#include "common/protobuf/utility.h"
#include "common/uri_impl.h"
#include "common/utility.h"

#include "absl/strings/str_split.h"
#include "api/client/options.pb.validate.h"
#include "tclap/CmdLine.h"

namespace Nighthawk {
namespace Client {

OptionsImpl::OptionsImpl(int argc, const char* const* argv) {
  setNonTrivialDefaults();
  // Override some defaults, we are in CLI-mode.
  verbosity_ = "info";
  output_format_ = "human";

  const char* descr = "L7 (HTTP/HTTPS/HTTP2) performance characterization tool.";
  TCLAP::CmdLine cmd(descr, ' ', "PoC"); // NOLINT

  TCLAP::ValueArg<uint32_t> requests_per_second(
      "", "rps",
      fmt::format("The target requests-per-second rate. Default: {}.", requests_per_second_), false,
      0, "uint32_t", cmd);
  TCLAP::ValueArg<uint32_t> connections(
      "", "connections",
      fmt::format("The number of connections per event loop that the test should maximally "
                  "use. HTTP/1 only. Default: {}.",
                  connections_),
      false, 0, "uint32_t", cmd);
  TCLAP::ValueArg<uint32_t> duration(
      "", "duration",
      fmt::format("The number of seconds that the test should run. Default: {}.", duration_), false,
      0, "uint32_t", cmd);
  TCLAP::ValueArg<uint32_t> timeout(
      "", "timeout",
      fmt::format(
          "Timeout period in seconds used for both connection timeout and grace period waiting for "
          "lagging responses to come in after the test run is done. Default: {}.",
          timeout_),
      false, 0, "uint32_t", cmd);

  TCLAP::SwitchArg h2("", "h2", "Use HTTP/2", cmd);

  TCLAP::ValueArg<std::string> concurrency(
      "", "concurrency",
      fmt::format(
          "The number of concurrent event loops that should be used. Specify 'auto' to let "
          "Nighthawk leverage all vCPUs that have affinity to the Nighthawk process.Note that "
          "increasing this results in an effective load multiplier combined with the configured-- "
          "rps "
          "and --connections values. Default: {}. ",
          concurrency_),
      false, "", "string", cmd);

  std::vector<std::string> log_levels = {"trace", "debug", "info", "warn", "error", "critical"};
  TCLAP::ValuesConstraint<std::string> verbosities_allowed(log_levels);

  TCLAP::ValueArg<std::string> verbosity(
      "v", "verbosity",
      fmt::format("Verbosity of the output. Possible values: [trace, debug, info, warn, error, "
                  "critical]. The "
                  "default level is '{}'.",
                  verbosity_),
      false, "", &verbosities_allowed, cmd);

  std::vector<std::string> output_formats = {"human", "yaml", "json"};
  TCLAP::ValuesConstraint<std::string> output_formats_allowed(output_formats);

  TCLAP::ValueArg<std::string> output_format(
      "", "output-format",
      fmt::format("Verbosity of the output. Possible values: [human, yaml, json]. The "
                  "default output format is '{}'.",
                  output_format_),
      false, "", &output_formats_allowed, cmd);

  TCLAP::SwitchArg prefetch_connections(
      "", "prefetch-connections", "Prefetch connections before benchmarking (HTTP/1 only).", cmd);

  // Note: we allow a burst size of 1, which intuitively may not make sense. However, allowing it
  // doesn't hurt either, and it does allow one to use a the same code-execution-paths in test
  // series that ramp up burst sizes.
  TCLAP::ValueArg<uint32_t> burst_size(
      "", "burst-size",
      fmt::format("Release requests in bursts of the specified size (default: {}).", burst_size_),
      false, 0, "uint32_t", cmd);
  std::vector<std::string> address_families = {"auto", "v4", "v6"};
  TCLAP::ValuesConstraint<std::string> address_families_allowed(address_families);
  TCLAP::ValueArg<std::string> address_family(
      "", "address-family",
      fmt::format("Network addres family preference. Possible values: [auto, v4, v6]. The "
                  "default output format is '{}'.",
                  address_family_),
      false, "", &address_families_allowed, cmd);

  std::vector<std::string> request_methods = {"GET",    "HEAD",    "POST",    "PUT",
                                              "DELETE", "CONNECT", "OPTIONS", "TRACE"};
  TCLAP::ValuesConstraint<std::string> request_methods_allowed(request_methods);
  TCLAP::ValueArg<std::string> request_method("", "request-method",
                                              "Request method used when sending requests. The "
                                              "default is 'GET'.",
                                              false, "GET", &request_methods_allowed, cmd);

  TCLAP::MultiArg<std::string> request_headers("", "request-header",
                                               "Raw request headers in the format of 'name: value' "
                                               "pairs. This argument may specified multiple times.",
                                               false, "string", cmd);
  TCLAP::ValueArg<uint32_t> request_body_size(
      "", "request-body-size",
      "Size of the request body to send. NH will send a number of consecutive 'a' characters equal "
      "to the number specified here. (default: 0, no data).",
      false, 0, "uint32_t", cmd);

  TCLAP::ValueArg<std::string> tls_context(
      "", "tls-context",
      "Tls context configuration in yaml or json. Example (json):"
      "{common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-AES128-SHA\"]}}}",
      false, "", "string", cmd);

  TCLAP::ValueArg<uint32_t> max_pending_requests(
      "", "max-pending-requests",
      "Max pending requests (default: 1, no client side queuing. Specifying any other value will "
      "allow client-side queuing of requests).",
      false, 0, "uint32_t", cmd);

  TCLAP::ValueArg<uint32_t> max_active_requests(
      "", "max-active-requests",
      fmt::format("Max active requests (default: {}).", max_active_requests_), false, 0, "uint32_t",
      cmd);

  TCLAP::ValueArg<uint32_t> max_requests_per_connection(
      "", "max-requests-per-connection",
      fmt::format("Max requests per connection (default: {}).", max_requests_per_connection_),
      false, 0, "uint32_t", cmd);

  TCLAP::UnlabeledValueArg<std::string> uri("uri",
                                            "uri to benchmark. http:// and https:// are supported, "
                                            "but in case of https no certificates are validated.",
                                            true, "", "uri format", cmd);

  cmd.setExceptionHandling(false);
  try {
    cmd.parse(argc, argv);
  } catch (TCLAP::ArgException& e) {
    try {
      cmd.getOutput()->failure(cmd, e);
    } catch (const TCLAP::ExitException&) {
      // failure() has already written an informative message to stderr, so all that's left to do
      // is throw our own exception with the original message.
      throw MalformedArgvException(e.what());
    }
  } catch (const TCLAP::ExitException& e) {
    // parse() throws an ExitException with status 0 after printing the output for --help and
    // --version.
    throw NoServingException();
  }

  if (requests_per_second.isSet()) {
    requests_per_second_ = requests_per_second.getValue();
  }
  if (connections.isSet()) {
    connections_ = connections.getValue();
  }
  if (duration.isSet()) {
    duration_ = duration.getValue();
  }
  if (timeout.isSet()) {
    timeout_ = timeout.getValue();
  }
  uri_ = uri.getValue();
  if (h2.isSet()) {
    h2_ = h2.getValue();
  }
  if (concurrency.isSet()) {
    concurrency_ = concurrency.getValue();
  }
  if (verbosity.isSet()) {
    verbosity_ = verbosity.getValue();
  }
  if (output_format.isSet()) {
    output_format_ = output_format.getValue();
  }
  if (prefetch_connections.isSet()) {
    prefetch_connections_ = prefetch_connections.getValue();
  }
  if (burst_size.isSet()) {
    burst_size_ = burst_size.getValue();
  }
  if (address_family.isSet()) {
    address_family_ = address_family.getValue();
  }
  if (request_method.isSet()) {
    request_method_ = request_method.getValue();
  }
  if (request_headers.isSet()) {
    request_headers_ = request_headers.getValue();
  }
  if (request_body_size.isSet()) {
    request_body_size_ = request_body_size.getValue();
  }
  if (max_pending_requests.isSet()) {
    max_pending_requests_ = max_pending_requests.getValue();
  }
  if (max_active_requests.isSet()) {
    max_active_requests_ = max_active_requests.getValue();
  }
  if (max_requests_per_connection.isSet()) {
    max_requests_per_connection_ = max_requests_per_connection.getValue();
  }

  // CLI-specific tests.
  if (requests_per_second_ > largest_acceptable_uint32_option_value) {
    throw MalformedArgvException("Invalid value for --rps");
  }
  if (connections_ > largest_acceptable_uint32_option_value) {
    throw MalformedArgvException("Invalid value for --connections");
  }
  if (duration_ > largest_acceptable_uint32_option_value) {
    throw MalformedArgvException("Invalid value for --duration");
  }
  if (timeout_ > largest_acceptable_uint32_option_value) {
    throw MalformedArgvException("Invalid value for --timeout");
  }
  if (request_body_size_ > largest_acceptable_uint32_option_value) {
    throw MalformedArgvException("Invalid value for --request-body-size");
  }
  if (burst_size_ > largest_acceptable_uint32_option_value) {
    throw MalformedArgvException("Invalid value for --burst-size");
  }
  if (max_pending_requests_ > largest_acceptable_uint32_option_value) {
    throw MalformedArgvException("Invalid value for --max-pending-requests");
  }
  if (max_active_requests_ > largest_acceptable_uint32_option_value) {
    throw MalformedArgvException("Invalid value for --max-active-requests");
  }
  if (max_requests_per_connection_ > largest_acceptable_uint32_option_value) {
    throw MalformedArgvException("Invalid value for --max-requests-per-connection");
  }

  if (!tls_context.getValue().empty()) {
    // TODO(oschaaf): used to by loadFromJsonEx, which is now gone.
    Envoy::MessageUtil::loadFromJson(tls_context.getValue(), tls_context_,
                                     Envoy::ProtobufMessage::getNullValidationVisitor());
  }
  validate();
}

OptionsImpl::OptionsImpl(const nighthawk::client::CommandLineOptions& options) {
  // XXX(oschaaf): as default composition isn't trivial, add tests to ensure all constructors
  setNonTrivialDefaults();

  for (const auto& header : options.request_options().request_headers()) {
    std::string header_string =
        fmt::format("{}:{}", header.header().key(), header.header().value());
    request_headers_.push_back(header_string);
  }

  requests_per_second_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, requests_per_second, requests_per_second_);
  if (options.has_duration()) {
    duration_ = options.duration().seconds();
  }
  if (options.has_timeout()) {
    timeout_ = options.timeout().seconds();
  }
  uri_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, uri, uri_);
  h2_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, h2, h2_);
  concurrency_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, concurrency, concurrency_);
  verbosity_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, verbosity, verbosity_);
  output_format_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, output_format, output_format_);
  prefetch_connections_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, prefetch_connections, prefetch_connections_);
  burst_size_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, burst_size, burst_size_);
  address_family_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, address_family, address_family_);
  const auto& request_options = options.request_options();
  if (request_options.request_method() !=
      ::envoy::api::v2::core::RequestMethod::METHOD_UNSPECIFIED) {
    request_method_ = ::envoy::api::v2::core::RequestMethod_Name(request_options.request_method());
  }
  request_body_size_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(request_options, request_body_size, request_body_size_);
  max_pending_requests_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, max_pending_requests, max_pending_requests_);
  max_active_requests_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, max_active_requests, max_active_requests_);
  max_requests_per_connection_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(
      options, max_requests_per_connection, max_requests_per_connection_);
  connections_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, connections, connections_);

  tls_context_.MergeFrom(options.tls_context());
  validate();
}

void OptionsImpl::setNonTrivialDefaults() {
  concurrency_ = "1";
  verbosity_ = "warn";
  output_format_ = "json";
  address_family_ = "v4";
  request_method_ = "GET";
}

void OptionsImpl::validate() const {
  // concurrency must be either 'auto' or a positive integer.
  if (concurrency_ != "auto") {
    int parsed_concurrency;
    try {
      parsed_concurrency = std::stoi(concurrency_);
    } catch (const std::invalid_argument& ia) {
      throw MalformedArgvException("Invalid value for --concurrency");
    } catch (const std::out_of_range& oor) {
      throw MalformedArgvException("Value out of range: --concurrency");
    }
    if (parsed_concurrency <= 0) {
      throw MalformedArgvException("Value for --concurrency should be greater then 0.");
    }
  }
  try {
    UriImpl uri(uri_);
  } catch (const UriException) {
    throw MalformedArgvException("Invalid URI");
  }
  try {
    Envoy::MessageUtil::validate(*toCommandLineOptions());
  } catch (const Envoy::ProtoValidationException& e) {
    throw MalformedArgvException(e.what());
  }
}

CommandLineOptionsPtr OptionsImpl::toCommandLineOptions() const {
  CommandLineOptionsPtr command_line_options =
      std::make_unique<nighthawk::client::CommandLineOptions>();

  command_line_options->mutable_connections()->set_value(connections());
  command_line_options->mutable_duration()->set_seconds(duration().count());
  command_line_options->mutable_requests_per_second()->set_value(requestsPerSecond());
  command_line_options->mutable_timeout()->set_seconds(timeout().count());
  command_line_options->mutable_h2()->set_value(h2());
  command_line_options->mutable_uri()->set_value(uri());
  command_line_options->mutable_concurrency()->set_value(concurrency());
  command_line_options->mutable_verbosity()->set_value(verbosity());
  command_line_options->mutable_output_format()->set_value(outputFormat());
  command_line_options->mutable_prefetch_connections()->set_value(prefetchConnections());
  command_line_options->mutable_burst_size()->set_value(burstSize());
  command_line_options->mutable_address_family()->set_value(addressFamily());
  auto request_options = command_line_options->mutable_request_options();
  envoy::api::v2::core::RequestMethod method =
      envoy::api::v2::core::RequestMethod::METHOD_UNSPECIFIED;
  envoy::api::v2::core::RequestMethod_Parse(requestMethod(), &method);
  request_options->set_request_method(method);
  for (const auto& header : requestHeaders()) {
    auto header_value_option = request_options->add_request_headers();
    // TODO(oschaaf): expose append option in CLI? For now we just set.
    header_value_option->mutable_append()->set_value(false);
    auto request_header = header_value_option->mutable_header();
    std::vector<std::string> split_header = absl::StrSplit(
        header, ':',
        absl::SkipWhitespace()); // TODO(oschaaf): maybe throw when we find > 2 elements.
    request_header->set_key(split_header[0]);
    if (split_header.size() == 2) {
      request_header->set_value(split_header[1]);
    }
  }
  request_options->mutable_request_body_size()->set_value(requestBodySize());
  *(command_line_options->mutable_tls_context()) = tlsContext();
  command_line_options->mutable_max_pending_requests()->set_value(maxPendingRequests());
  command_line_options->mutable_max_active_requests()->set_value(maxActiveRequests());
  command_line_options->mutable_max_requests_per_connection()->set_value(
      maxRequestsPerConnection());
  return command_line_options;
}

} // namespace Client
} // namespace Nighthawk
