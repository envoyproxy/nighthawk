#include "client/options_impl.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "api/client/options.pb.validate.h"

#include "common/uri_impl.h"
#include "common/utility.h"

#include "client/output_formatter_impl.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "fmt/ranges.h"

namespace Nighthawk {
namespace Client {

#define TCLAP_SET_IF_SPECIFIED(command, value_member)                                              \
  ((value_member) = (((command).isSet()) ? ((command).getValue()) : (value_member)))

OptionsImpl::OptionsImpl(int argc, const char* const* argv) {
  setNonTrivialDefaults();
  // Override some defaults, we are in CLI-mode.
  verbosity_ = nighthawk::client::Verbosity::INFO;
  output_format_ = nighthawk::client::OutputFormat::HUMAN;

  // TODO(oschaaf): Purge the validation we perform here. Most of it should have become
  // redundant now that we also perform validation of the resulting proto.
  const char* descr = "L7 (HTTP/HTTPS/HTTP2) performance characterization tool.";
  TCLAP::CmdLine cmd(descr, ' ', "PoC"); // NOLINT

  // Any default values we pass into TCLAP argument declarations are arbitrary, as we do not rely on
  // TCLAP for providing default values. Default values are declared in and sourced from
  // options_impl.h, modulo non-trivial data types (see setNonTrivialDefaults()).
  TCLAP::ValueArg<uint32_t> requests_per_second(
      "", "rps",
      fmt::format("The target requests-per-second rate. Default: {}.", requests_per_second_), false,
      0, "uint32_t", cmd);
  TCLAP::ValueArg<uint32_t> connections(
      "", "connections",
      fmt::format("The maximum allowed number of concurrent connections per event loop. HTTP/1 "
                  "only. Default: {}.",
                  connections_),
      false, 0, "uint32_t", cmd);
  TCLAP::ValueArg<uint32_t> duration(
      "", "duration",
      fmt::format("The number of seconds that the test should run. Default: {}.", duration_), false,
      0, "uint32_t", cmd);
  TCLAP::ValueArg<uint32_t> timeout(
      "", "timeout",
      fmt::format("Connection connect timeout period in seconds. Default: {}.", timeout_), false, 0,
      "uint32_t", cmd);

  TCLAP::SwitchArg h2("", "h2", "Use HTTP/2", cmd);

  TCLAP::ValueArg<std::string> concurrency(
      "", "concurrency",
      fmt::format(
          "The number of concurrent event loops that should be used. Specify 'auto' to let "
          "Nighthawk leverage all vCPUs that have affinity to the Nighthawk process. Note that "
          "increasing this results in an effective load multiplier combined with the configured "
          "--rps and --connections values. Default: {}. ",
          concurrency_),
      false, "", "string", cmd);

  std::vector<std::string> log_levels = {"trace", "debug", "info", "warn", "error", "critical"};
  TCLAP::ValuesConstraint<std::string> verbosities_allowed(log_levels);

  TCLAP::ValueArg<std::string> verbosity(
      "v", "verbosity",
      fmt::format(
          "Verbosity of the output. Possible values: [trace, debug, info, warn, error, "
          "critical]. The "
          "default level is '{}'.",
          absl::AsciiStrToLower(nighthawk::client::Verbosity_VerbosityOptions_Name(verbosity_))),
      false, "", &verbosities_allowed, cmd);

  std::vector<std::string> output_formats = OutputFormatterImpl::getLowerCaseOutputFormats();
  TCLAP::ValuesConstraint<std::string> output_formats_allowed(output_formats);
  TCLAP::ValueArg<std::string> output_format(
      "", "output-format",
      fmt::format("Output format. Possible values: {}. The "
                  "default output format is '{}'.",
                  output_formats,
                  absl::AsciiStrToLower(
                      nighthawk::client::OutputFormat_OutputFormatOptions_Name(output_format_))),
      false, "", &output_formats_allowed, cmd);

  TCLAP::SwitchArg prefetch_connections(                     // NOLINT
      "", "prefetch-connections",                            // NOLINT
      "Use proactive connection prefetching (HTTP/1 only).", // NOLINT
      cmd);                                                  // NOLINT

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
      fmt::format("Network address family preference. Possible values: [auto, v4, v6]. The "
                  "default output format is '{}'.",
                  nighthawk::client::AddressFamily::AddressFamilyOptions_Name(address_family_)),
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
      fmt::format("Max pending requests (default: {}, no client side queuing. Specifying any other "
                  "value will "
                  "allow client-side queuing of requests).",
                  max_pending_requests_),
      false, 0, "uint32_t", cmd);

  TCLAP::ValueArg<uint32_t> max_active_requests(
      "", "max-active-requests",
      fmt::format(
          "The maximum allowed number of concurrently active requests. HTTP/2 only. (default: {}).",
          max_active_requests_),
      false, 0, "uint32_t", cmd);
  // NOLINTNEXTLINE
  TCLAP::ValueArg<uint32_t> max_requests_per_connection(
      "", "max-requests-per-connection",
      fmt::format("Max requests per connection (default: {}).", max_requests_per_connection_),
      false, 0, "uint32_t", cmd);

  std::vector<std::string> sequencer_idle_strategies = {"spin", "poll", "sleep"};
  TCLAP::ValuesConstraint<std::string> sequencer_idle_strategies_allowed(sequencer_idle_strategies);
  TCLAP::ValueArg<std::string> sequencer_idle_strategy(
      "", "sequencer-idle-strategy",
      fmt::format(
          "Choose between using a busy spin/yield loop or have the thread poll or sleep while "
          "waiting for the next scheduled request (default: {}).",
          absl::AsciiStrToLower(
              nighthawk::client::SequencerIdleStrategy_SequencerIdleStrategyOptions_Name(
                  sequencer_idle_strategy_))),
      false, "", &sequencer_idle_strategies_allowed, cmd);

  TCLAP::ValueArg<std::string> trace(
      "", "trace", "Trace uri. Example: zipkin://localhost:9411/api/v1/spans. Default is empty.",
      false, "", "uri format", cmd);
  TCLAP::MultiArg<std::string> termination_predicates(
      "", "termination-predicate",
      "Termination predicate. Allows specifying a counter name plus threshold value for "
      "terminating execution.",
      false, "<string, uint64_t>", cmd);
  TCLAP::MultiArg<std::string> failure_predicates(
      "", "failure-predicate",
      "Failure predicate. Allows specifying a counter name plus threshold value for "
      "failing execution. Defaults to not tolerating error status codes and connection errors.",
      false, "<string, uint64_t>", cmd);

  TCLAP::SwitchArg open_loop(
      "", "open-loop",
      "Enable open loop mode. When enabled, the benchmark client will not provide backpressure "
      "when resource limits are hit.",
      cmd);
  TCLAP::ValueArg<std::string> jitter_uniform(
      "", "jitter-uniform",
      "Add uniformly distributed absolute request-release timing jitter. For example, to add 10 us "
      "of jitter, specify .00001s. Default is empty / no uniform jitter.",
      false, "", "duration", cmd);
  TCLAP::MultiArg<std::string> backend_endpoints(
      "", "backend-endpoint",
      "Backend endpoint overrides. "
      "This argument is intended to be specified multiple times. "
      "Nighthawk will generate the same traffic as usual, but "
      "will spread it across all backend endpoints with "
      "round robin distribution. "
      "Endpoint formats: IPv4:port, [IPv6]:port, DNS:port. "
      "Note: The host and port from the URI are ignored when "
      "--backend-endpoint is present. ",
      false, "string", cmd);
  TCLAP::UnlabeledValueArg<std::string> uri("uri",
                                            "uri to benchmark. http:// and https:// are supported, "
                                            "but in case of https no certificates are validated.",
                                            true, "", "uri format", cmd);

  Utility::parseCommand(cmd, argc, argv);

  TCLAP_SET_IF_SPECIFIED(requests_per_second, requests_per_second_);
  TCLAP_SET_IF_SPECIFIED(connections, connections_);
  TCLAP_SET_IF_SPECIFIED(duration, duration_);
  TCLAP_SET_IF_SPECIFIED(timeout, timeout_);
  uri_ = uri.getValue();
  TCLAP_SET_IF_SPECIFIED(h2, h2_);
  TCLAP_SET_IF_SPECIFIED(concurrency, concurrency_);
  // TODO(oschaaf): is there a generic way to set these enum values?
  if (verbosity.isSet()) {
    std::string upper_cased = verbosity.getValue();
    absl::AsciiStrToUpper(&upper_cased);
    RELEASE_ASSERT(nighthawk::client::Verbosity::VerbosityOptions_Parse(upper_cased, &verbosity_),
                   "Failed to parse verbosity");
  }
  if (output_format.isSet()) {
    std::string upper_cased = output_format.getValue();
    absl::AsciiStrToUpper(&upper_cased);
    RELEASE_ASSERT(
        nighthawk::client::OutputFormat::OutputFormatOptions_Parse(upper_cased, &output_format_),
        "Failed to parse output format");
  }
  TCLAP_SET_IF_SPECIFIED(prefetch_connections, prefetch_connections_);
  TCLAP_SET_IF_SPECIFIED(burst_size, burst_size_);
  if (address_family.isSet()) {
    std::string upper_cased = address_family.getValue();
    absl::AsciiStrToUpper(&upper_cased);
    RELEASE_ASSERT(
        nighthawk::client::AddressFamily::AddressFamilyOptions_Parse(upper_cased, &address_family_),
        "Failed to parse address family");
  }
  if (request_method.isSet()) {
    std::string upper_cased = request_method.getValue();
    absl::AsciiStrToUpper(&upper_cased);
    RELEASE_ASSERT(envoy::api::v2::core::RequestMethod_Parse(upper_cased, &request_method_),
                   "Failed to parse request method");
  }
  TCLAP_SET_IF_SPECIFIED(request_headers, request_headers_);
  TCLAP_SET_IF_SPECIFIED(request_body_size, request_body_size_);
  TCLAP_SET_IF_SPECIFIED(max_pending_requests, max_pending_requests_);
  TCLAP_SET_IF_SPECIFIED(max_active_requests, max_active_requests_);
  TCLAP_SET_IF_SPECIFIED(max_requests_per_connection, max_requests_per_connection_);
  if (sequencer_idle_strategy.isSet()) {
    std::string upper_cased = sequencer_idle_strategy.getValue();
    absl::AsciiStrToUpper(&upper_cased);
    RELEASE_ASSERT(nighthawk::client::SequencerIdleStrategy::SequencerIdleStrategyOptions_Parse(
                       upper_cased, &sequencer_idle_strategy_),
                   "Failed to parse sequencer idle strategy");
  }
  TCLAP_SET_IF_SPECIFIED(trace, trace_);
  parsePredicates(termination_predicates, termination_predicates_);
  parsePredicates(failure_predicates, failure_predicates_);
  TCLAP_SET_IF_SPECIFIED(open_loop, open_loop_);
  if (jitter_uniform.isSet()) {
    Envoy::ProtobufWkt::Duration duration;
    if (Envoy::Protobuf::util::TimeUtil::FromString(jitter_uniform.getValue(), &duration)) {
      if (duration.nanos() > 0 || duration.seconds() > 0) {
        jitter_uniform_ = std::chrono::nanoseconds(
            Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(duration));
      } else {
        throw MalformedArgvException("--jitter-uniform is out of range");
      }
    } else {
      throw MalformedArgvException("Invalid value for --jitter-uniform");
    }
  }
  TCLAP_SET_IF_SPECIFIED(backend_endpoints, backend_endpoints_);

  // CLI-specific tests.
  // TODO(oschaaf): as per mergconflicts's remark, it would be nice to aggregate
  // these and present everything we couldn't understand to the CLI user in on go.
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
    try {
      Envoy::MessageUtil::loadFromJson(tls_context.getValue(), tls_context_,
                                       Envoy::ProtobufMessage::getStrictValidationVisitor());
    } catch (const Envoy::EnvoyException& e) {
      throw MalformedArgvException(e.what());
    }
  }
  validate();
}

void OptionsImpl::parsePredicates(const TCLAP::MultiArg<std::string>& arg,
                                  TerminationPredicateMap& predicates) {
  if (arg.isSet()) {
    predicates.clear();
  }
  for (const auto& predicate : arg) {
    std::vector<std::string> split_predicate =
        absl::StrSplit(predicate, ':', absl::SkipWhitespace());
    if (split_predicate.size() != 2) {
      throw MalformedArgvException(
          fmt::format("Termination predicate '{}' is badly formatted.", predicate));
    }

    uint32_t threshold = 0;
    if (absl::SimpleAtoi(split_predicate[1], &threshold)) {
      predicates[split_predicate[0]] = threshold;
    } else {
      throw MalformedArgvException(
          fmt::format("Termination predicate '{}' has an out of range threshold.", predicate));
    }
  }
}

OptionsImpl::OptionsImpl(const nighthawk::client::CommandLineOptions& options) {
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
    request_method_ = request_options.request_method();
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
  sequencer_idle_strategy_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, sequencer_idle_strategy, sequencer_idle_strategy_);
  trace_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, trace, trace_);
  open_loop_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, open_loop, open_loop_);
  tls_context_.MergeFrom(options.tls_context());
  if (options.failure_predicates().size()) {
    failure_predicates_.clear();
  }
  for (const auto& predicate : options.failure_predicates()) {
    failure_predicates_[predicate.first] = predicate.second;
  }
  for (const auto& predicate : options.termination_predicates()) {
    termination_predicates_[predicate.first] = predicate.second;
  }
  if (options.has_jitter_uniform()) {
    jitter_uniform_ = std::chrono::nanoseconds(
        Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(options.jitter_uniform()));
  }
  for (const auto& backend_endpoint : options.backend_endpoints()) {
    backend_endpoints_.push_back(backend_endpoint.value());
  }
  validate();
}

void OptionsImpl::setNonTrivialDefaults() {
  concurrency_ = "1";
  // By default, we don't tolerate error status codes and connection failures, and will report
  // upon observing those.
  failure_predicates_["benchmark.http_4xx"] = 0;
  failure_predicates_["benchmark.http_5xx"] = 0;
  failure_predicates_["benchmark.pool_connection_failure"] = 0;
  jitter_uniform_ = std::chrono::nanoseconds(0);
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
  if (!backend_endpoints_.empty()) {
    HostAddressType first_host_address_type =
        Utility::hostAddressTypeFromHostPort(backend_endpoints_[0]);
    for (const std::string& backend_endpoint : backend_endpoints_) {
      HostAddressType host_address_type = Utility::hostAddressTypeFromHostPort(backend_endpoint);
      if (host_address_type == HostAddressType::INVALID) {
        throw MalformedArgvException(fmt::format("--backend-endpoint addresses must be in the form "
                                                 "IPv4:port, [IPv6]:port, or DNS:port. Got '{}'.",
                                                 backend_endpoint));
      }
      if (host_address_type != first_host_address_type) {
        throw MalformedArgvException("All --backend-endpoint values must be the same address type "
                                     "(IPv4:port, [IPv6]:port, or DNS:port).");
      }
    }
  }
  try {
    UriImpl uri(uri_);
  } catch (const UriException&) {
    throw MalformedArgvException("Invalid URI");
  }
  try {
    Envoy::MessageUtil::validate(*toCommandLineOptions(),
                                 Envoy::ProtobufMessage::getStrictValidationVisitor());
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
  command_line_options->mutable_address_family()->set_value(
      static_cast<nighthawk::client::AddressFamily_AddressFamilyOptions>(addressFamily()));
  auto request_options = command_line_options->mutable_request_options();
  request_options->set_request_method(requestMethod());
  for (const auto& header : requestHeaders()) {
    auto header_value_option = request_options->add_request_headers();
    // TODO(oschaaf): expose append option in CLI? For now we just set.
    header_value_option->mutable_append()->set_value(false);
    auto request_header = header_value_option->mutable_header();
    auto pos = header.find(':');
    if (pos != std::string::npos) {
      request_header->set_key(header.substr(0, pos));
      // Any visible char, including ':', is allowed in header values.
      request_header->set_value(header.substr(pos + 1));
    } else {
      throw MalformedArgvException("A ':' is required in a header.");
    }
  }
  request_options->mutable_request_body_size()->set_value(requestBodySize());
  *(command_line_options->mutable_tls_context()) = tlsContext();
  command_line_options->mutable_max_pending_requests()->set_value(maxPendingRequests());
  command_line_options->mutable_max_active_requests()->set_value(maxActiveRequests());
  command_line_options->mutable_max_requests_per_connection()->set_value(
      maxRequestsPerConnection());
  command_line_options->mutable_sequencer_idle_strategy()->set_value(sequencerIdleStrategy());
  command_line_options->mutable_trace()->set_value(trace());
  auto termination_predicates_option = command_line_options->mutable_termination_predicates();
  for (const auto& predicate : terminationPredicates()) {
    termination_predicates_option->insert({predicate.first, predicate.second});
  }
  auto failure_predicates_option = command_line_options->mutable_failure_predicates();
  for (const auto& predicate : failurePredicates()) {
    failure_predicates_option->insert({predicate.first, predicate.second});
  }
  command_line_options->mutable_open_loop()->set_value(openLoop());
  if (jitterUniform().count() > 0) {
    *command_line_options->mutable_jitter_uniform() =
        Envoy::Protobuf::util::TimeUtil::NanosecondsToDuration(jitterUniform().count());
  }
  for (const std::string& backend_endpoint : backendEndpoints()) {
    command_line_options->add_backend_endpoints()->set_value(backend_endpoint);
  }
  return command_line_options;
}

} // namespace Client
} // namespace Nighthawk
