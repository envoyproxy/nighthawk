#include "client/options_impl.h"

#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "api/client/options.pb.validate.h"

#include "common/uri_impl.h"
#include "common/utility.h"
#include "common/version_info.h"

#include "client/output_formatter_impl.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
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
  TCLAP::CmdLine cmd(descr, ' ', VersionInfo::version()); // NOLINT

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
      fmt::format("The number of seconds that the test should run. "
                  "Default: {}. Mutually exclusive with --no-duration.",
                  duration_),
      false, 0, "uint32_t", cmd);
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
      "DEPRECATED, use --transport-socket instead. "
      "Tls context configuration in json or compact yaml. "
      "Mutually exclusive with --transport-socket. Example (json): "
      "{common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-AES128-SHA\"]}}}",
      false, "", "string", cmd);

  TCLAP::ValueArg<std::string> transport_socket(
      "", "transport-socket",
      "Transport socket configuration in json or compact yaml. "
      "Mutually exclusive with --tls-context. Example (json): "
      "{name:\"envoy.transport_sockets.tls\",typed_config:{"
      "\"@type\":\"type.googleapis.com/"
      "envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext\","
      "common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-AES128-SHA\"]}}}}",
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
      false, "string, uint64_t", cmd);
  TCLAP::MultiArg<std::string> failure_predicates(
      "", "failure-predicate",
      "Failure predicate. Allows specifying a counter name plus threshold value for "
      "failing execution. Defaults to not tolerating error status codes and connection errors.",
      false, "string, uint64_t", cmd);

  std::vector<std::string> h1_connection_reuse_strategies = {"mru", "lru"};
  TCLAP::ValuesConstraint<std::string> h1_connection_reuse_strategies_allowed(
      h1_connection_reuse_strategies);
  TCLAP::ValueArg<std::string> experimental_h1_connection_reuse_strategy(
      "", "experimental-h1-connection-reuse-strategy",
      fmt::format(
          "Choose picking the most recently used, or least-recently-used connections for re-use."
          "(default: {}). WARNING: this option is experimental and may be removed or changed in "
          "the future!",
          absl::AsciiStrToLower(
              nighthawk::client::H1ConnectionReuseStrategy_H1ConnectionReuseStrategyOptions_Name(
                  experimental_h1_connection_reuse_strategy_))),
      false, "", &h1_connection_reuse_strategies_allowed, cmd);
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
  TCLAP::ValueArg<std::string> nighthawk_service(
      "", "nighthawk-service",
      "Nighthawk service uri. Example: grpc://localhost:8843/. Default is empty.", false, "",
      "uri format", cmd);
  TCLAP::SwitchArg h2_use_multiple_connections(
      "", "experimental-h2-use-multiple-connections",
      "Use experimental HTTP/2 pool which will use multiple connections. WARNING: feature may be "
      "removed or changed in the future!",
      cmd);

  TCLAP::MultiArg<std::string> multi_target_endpoints(
      "", "multi-target-endpoint",
      "Target endpoint in the form IPv4:port, [IPv6]:port, or DNS:port. "
      "This argument is intended to be specified multiple times. "
      "Nighthawk will spread traffic across all endpoints with "
      "round robin distribution. "
      "Mutually exclusive with providing a URI.",
      false, "string", cmd);
  TCLAP::ValueArg<std::string> multi_target_path(
      "", "multi-target-path",
      "The single absolute path Nighthawk should request from each target endpoint. "
      "Required when using --multi-target-endpoint. "
      "Mutually exclusive with providing a URI.",
      false, "", "string", cmd);
  TCLAP::SwitchArg multi_target_use_https(
      "", "multi-target-use-https",
      "Use HTTPS to connect to the target endpoints. Otherwise HTTP is used. "
      "Mutually exclusive with providing a URI.",
      cmd);

  TCLAP::MultiArg<std::string> labels("", "label",
                                      "Label. Allows specifying multiple labels which will be "
                                      "persisted in structured output formats.",
                                      false, "string", cmd);

  TCLAP::UnlabeledValueArg<std::string> uri(
      "uri",
      "URI to benchmark. http:// and https:// are supported, "
      "but in case of https no certificates are validated. "
      "Provide a URI when you need to benchmark a single endpoint. For multiple "
      "endpoints, set --multi-target-* instead.",
      false, "", "uri format", cmd);

  TCLAP::ValueArg<std::string> request_source(
      "", "request-source",
      "Remote gRPC source that will deliver to-be-replayed traffic. Each worker will separately "
      "connect to this source. For example grpc://127.0.0.1:8443/. "
      "Mutually exclusive with --request_source_plugin_config.",
      false, "", "uri format", cmd);
  TCLAP::ValueArg<std::string> request_source_plugin_config(
      "", "request-source-plugin-config",
      "[Request "
      "Source](https://github.com/envoyproxy/nighthawk/blob/master/docs/root/"
      "overview.md#requestsource) plugin configuration in json or compact yaml. "
      "Mutually exclusive with --request-source. Example (json): "
      "{name:\"nighthawk.stub-request-source-plugin\",typed_config:{"
      "\"@type\":\"type.googleapis.com/nighthawk.request_source.StubPluginConfig\","
      "test_value:\"3\"}}",
      false, "", "string", cmd);
  TCLAP::SwitchArg simple_warmup(
      "", "simple-warmup",
      "Perform a simple single warmup request (per worker) before starting execution. Note that "
      "this will be reflected in the counters that Nighthawk writes to the output. Default is "
      "false.",
      cmd);
  TCLAP::SwitchArg no_duration(
      "", "no-duration",
      "Request infinite execution. Note that the default failure "
      "predicates will still be added. Mutually exclusive with --duration.",
      cmd);
  TCLAP::MultiArg<std::string> stats_sinks(
      "", "stats-sinks",
      "Stats sinks (in json or compact yaml) where Nighthawk "
      "metrics will be flushed. This argument is intended to "
      "be specified multiple times. Example (json): "
      "{name:\"envoy.stat_sinks.statsd\",typed_config:{\"@type\":\"type."
      "googleapis.com/"
      "envoy.config.metrics.v3.StatsdSink\",tcp_cluster_name:\"statsd\"}}",
      false, "string", cmd);

  TCLAP::ValueArg<uint32_t> stats_flush_interval(
      "", "stats-flush-interval",
      fmt::format("Time interval (in seconds) between flushes to configured "
                  "stats sinks. Default: {}.",
                  stats_flush_interval_),
      false, 5, "uint32_t", cmd);

  TCLAP::ValueArg<std::string> latency_response_header_name(
      "", "latency-response-header-name",
      "Set an optional header name that will be returned in responses, whose values will be "
      "tracked in a latency histogram if set. "
      "Can be used in tandem with the test server's response option "
      "\"emit_previous_request_delta_in_response_header\" to record elapsed time between request "
      "arrivals. "
      "Default: \"\"",
      false, "", "string", cmd);

  TCLAP::SwitchArg allow_envoy_deprecated_v2_api(
      "", "allow-envoy-deprecated-v2-api",
      "Set to allow usage of the v2 api. (Not recommended, support will stop in Q1 2021). Default: "
      "false",
      cmd);

  Utility::parseCommand(cmd, argc, argv);

  // --duration and --no-duration are mutually exclusive
  // Would love to have used cmd.xorAdd here, but that prevents
  // us from having a default duration when neither arg is specified,
  // as specifying one of those became mandatory.
  // That's why we manually validate this.
  if (duration.isSet() && (no_duration.isSet() && no_duration.getValue() == true)) {
    throw MalformedArgvException("--duration and --no-duration are mutually exclusive");
  }

  // Verify that if --stats-flush-interval is set, then --stats-sinks must also be set.
  if (stats_flush_interval.isSet() && !stats_sinks.isSet()) {
    throw MalformedArgvException(
        "if --stats-flush-interval is set, then --stats-sinks must also be set");
  }

  TCLAP_SET_IF_SPECIFIED(requests_per_second, requests_per_second_);
  TCLAP_SET_IF_SPECIFIED(connections, connections_);
  TCLAP_SET_IF_SPECIFIED(duration, duration_);
  TCLAP_SET_IF_SPECIFIED(timeout, timeout_);
  if (uri.isSet()) {
    uri_ = uri.getValue();
  }
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
    RELEASE_ASSERT(envoy::config::core::v3::RequestMethod_Parse(upper_cased, &request_method_),
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
  TCLAP_SET_IF_SPECIFIED(request_source, request_source_);

  if (experimental_h1_connection_reuse_strategy.isSet()) {
    std::string upper_cased = experimental_h1_connection_reuse_strategy.getValue();
    absl::AsciiStrToUpper(&upper_cased);
    const bool ok =
        nighthawk::client::H1ConnectionReuseStrategy::H1ConnectionReuseStrategyOptions_Parse(
            upper_cased, &experimental_h1_connection_reuse_strategy_);
    // TCLAP validation ought to have caught this earlier.
    RELEASE_ASSERT(ok, "Failed to parse h1 connection reuse strategy");
  }

  TCLAP_SET_IF_SPECIFIED(trace, trace_);
  parsePredicates(termination_predicates, termination_predicates_);
  parsePredicates(failure_predicates, failure_predicates_);
  TCLAP_SET_IF_SPECIFIED(open_loop, open_loop_);
  if (jitter_uniform.isSet()) {
    Envoy::ProtobufWkt::Duration duration;
    if (Envoy::Protobuf::util::TimeUtil::FromString(jitter_uniform.getValue(), &duration)) {
      if (duration.nanos() >= 0 && duration.seconds() >= 0) {
        jitter_uniform_ = std::chrono::nanoseconds(
            Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(duration));
      } else {
        throw MalformedArgvException("--jitter-uniform is out of range");
      }
    } else {
      throw MalformedArgvException("Invalid value for --jitter-uniform");
    }
  }
  TCLAP_SET_IF_SPECIFIED(nighthawk_service, nighthawk_service_);
  TCLAP_SET_IF_SPECIFIED(h2_use_multiple_connections, h2_use_multiple_connections_);
  TCLAP_SET_IF_SPECIFIED(multi_target_use_https, multi_target_use_https_);
  TCLAP_SET_IF_SPECIFIED(multi_target_path, multi_target_path_);
  if (multi_target_endpoints.isSet()) {
    for (const std::string& host_port : multi_target_endpoints.getValue()) {
      std::string host;
      int port;
      if (!Utility::parseHostPort(host_port, &host, &port)) {
        throw MalformedArgvException(fmt::format("--multi-target-endpoint must be in the format "
                                                 "IPv4:port, [IPv6]:port, or DNS:port. Got '{}'",
                                                 host_port));
      }
      nighthawk::client::MultiTarget::Endpoint endpoint;
      endpoint.mutable_address()->set_value(host);
      endpoint.mutable_port()->set_value(port);
      multi_target_endpoints_.push_back(endpoint);
    }
  }
  TCLAP_SET_IF_SPECIFIED(labels, labels_);
  TCLAP_SET_IF_SPECIFIED(simple_warmup, simple_warmup_);
  TCLAP_SET_IF_SPECIFIED(no_duration, no_duration_);
  if (stats_sinks.isSet()) {
    for (const std::string& stats_sink : stats_sinks.getValue()) {
      envoy::config::metrics::v3::StatsSink sink;
      try {
        Envoy::MessageUtil::loadFromJson(stats_sink, sink,
                                         Envoy::ProtobufMessage::getStrictValidationVisitor());
      } catch (const Envoy::EnvoyException& e) {
        throw MalformedArgvException(e.what());
      }
      stats_sinks_.push_back(sink);
    }
  }
  TCLAP_SET_IF_SPECIFIED(stats_flush_interval, stats_flush_interval_);
  TCLAP_SET_IF_SPECIFIED(latency_response_header_name, latency_response_header_name_);
  TCLAP_SET_IF_SPECIFIED(allow_envoy_deprecated_v2_api, allow_envoy_deprecated_v2_api_);

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
  if (stats_flush_interval_ > largest_acceptable_uint32_option_value) {
    throw MalformedArgvException("Invalid value for --stats-flush-interval");
  }

  if (!tls_context.getValue().empty()) {
    ENVOY_LOG(warn, "--tls-context is deprecated. "
                    "It can be replaced by an equivalent --transport-socket. "
                    "See --help for an example.");
  }
  if (!tls_context.getValue().empty() && !transport_socket.getValue().empty()) {
    throw MalformedArgvException("--tls-context and --transport-socket cannot both be set.");
  }
  if (!tls_context.getValue().empty()) {
    try {
      Envoy::MessageUtil::loadFromJson(tls_context.getValue(), tls_context_,
                                       Envoy::ProtobufMessage::getStrictValidationVisitor());
    } catch (const Envoy::EnvoyException& e) {
      throw MalformedArgvException(e.what());
    }
  }
  if (!transport_socket.getValue().empty()) {
    try {
      transport_socket_.emplace(envoy::config::core::v3::TransportSocket());
      Envoy::MessageUtil::loadFromJson(transport_socket.getValue(), transport_socket_.value(),
                                       Envoy::ProtobufMessage::getStrictValidationVisitor());
    } catch (const Envoy::EnvoyException& e) {
      throw MalformedArgvException(e.what());
    }
  }
  if (!request_source.getValue().empty() && !request_source_plugin_config.getValue().empty()) {
    throw MalformedArgvException(
        "--request-source and --request_source_plugin_config cannot both be set.");
  }
  if (!request_source_plugin_config.getValue().empty()) {
    try {
      request_source_plugin_config_.emplace(envoy::config::core::v3::TypedExtensionConfig());
      Envoy::MessageUtil::loadFromJson(request_source_plugin_config.getValue(),
                                       request_source_plugin_config_.value(),
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

  requests_per_second_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, requests_per_second, requests_per_second_);
  if (options.has_duration()) {
    duration_ = options.duration().seconds();
  }
  if (options.has_timeout()) {
    timeout_ = options.timeout().seconds();
  }
  if (options.has_uri()) {
    uri_ = options.uri().value();
  } else {
    multi_target_path_ =
        PROTOBUF_GET_WRAPPED_OR_DEFAULT(options.multi_target(), path, multi_target_path_);
    multi_target_use_https_ =
        PROTOBUF_GET_WRAPPED_OR_DEFAULT(options.multi_target(), use_https, multi_target_use_https_);
    for (const nighthawk::client::MultiTarget::Endpoint& endpoint :
         options.multi_target().endpoints()) {
      multi_target_endpoints_.push_back(endpoint);
    }
  }
  h2_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, h2, h2_);
  concurrency_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, concurrency, concurrency_);
  verbosity_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, verbosity, verbosity_);
  output_format_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, output_format, output_format_);
  prefetch_connections_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, prefetch_connections, prefetch_connections_);
  burst_size_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, burst_size, burst_size_);
  address_family_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, address_family, address_family_);

  if (options.has_request_options()) {
    const auto& request_options = options.request_options();
    for (const auto& header : request_options.request_headers()) {
      std::string header_string =
          fmt::format("{}:{}", header.header().key(), header.header().value());
      request_headers_.push_back(header_string);
    }
    if (request_options.request_method() !=
        ::envoy::config::core::v3::RequestMethod::METHOD_UNSPECIFIED) {
      request_method_ = request_options.request_method();
    }
    request_body_size_ =
        PROTOBUF_GET_WRAPPED_OR_DEFAULT(request_options, request_body_size, request_body_size_);
  } else if (options.has_request_source()) {
    const auto& request_source_options = options.request_source();
    request_source_ = request_source_options.uri();
  } else if (options.has_request_source_plugin_config()) {
    request_source_plugin_config_.emplace(envoy::config::core::v3::TypedExtensionConfig());
    request_source_plugin_config_.value().MergeFrom(options.request_source_plugin_config());
  }

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
  experimental_h1_connection_reuse_strategy_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, experimental_h1_connection_reuse_strategy,
                                      experimental_h1_connection_reuse_strategy_);
  open_loop_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, open_loop, open_loop_);

  tls_context_.MergeFrom(options.tls_context());

  if (options.has_transport_socket()) {
    transport_socket_.emplace(envoy::config::core::v3::TransportSocket());
    transport_socket_.value().MergeFrom(options.transport_socket());
  }

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
  for (const envoy::config::metrics::v3::StatsSink& stats_sink : options.stats_sinks()) {
    stats_sinks_.push_back(stats_sink);
  }
  stats_flush_interval_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, stats_flush_interval, stats_flush_interval_);
  nighthawk_service_ =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, nighthawk_service, nighthawk_service_);
  h2_use_multiple_connections_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(
      options, experimental_h2_use_multiple_connections, h2_use_multiple_connections_);
  simple_warmup_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, simple_warmup, simple_warmup_);
  if (options.has_no_duration()) {
    no_duration_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(options, no_duration, no_duration_);
  }
  std::copy(options.labels().begin(), options.labels().end(), std::back_inserter(labels_));
  latency_response_header_name_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(
      options, latency_response_header_name, latency_response_header_name_);
  allow_envoy_deprecated_v2_api_ = PROTOBUF_GET_WRAPPED_OR_DEFAULT(
      options, allow_envoy_deprecated_v2_api, allow_envoy_deprecated_v2_api_);
  if (options.has_scheduled_start()) {
    const auto elapsed_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::nanoseconds(options.scheduled_start().nanos()) +
        std::chrono::seconds(options.scheduled_start().seconds()));
    scheduled_start_ =
        Envoy::SystemTime(std::chrono::time_point<std::chrono::system_clock>(elapsed_since_epoch));
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
  // Also, fail fast when a remote request source is specified that we can't connect to or otherwise
  // fails.
  failure_predicates_["requestsource.upstream_rq_5xx"] = 0;
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
  if (request_source_ != "") {
    try {
      UriImpl uri(request_source_, "grpc");
      if (uri.scheme() != "grpc") {
        throw MalformedArgvException("Invalid replay source URI");
      }
    } catch (const UriException&) {
      throw MalformedArgvException("Invalid replay source URI");
    }
  }
  if (uri_.has_value()) {
    try {
      UriImpl uri(uri_.value());
    } catch (const UriException&) {
      throw MalformedArgvException(fmt::format("Invalid target URI: ''", uri_.value()));
    }
    if (!multi_target_endpoints_.empty() || !multi_target_path_.empty() ||
        multi_target_use_https_) {
      throw MalformedArgvException("URI and --multi-target-* options cannot both be specified.");
    }
  } else {
    if (multi_target_endpoints_.empty()) {
      throw MalformedArgvException("A URI or --multi-target-* options must be specified.");
    }
    if (multi_target_path_.empty()) {
      throw MalformedArgvException("--multi-target-path must be specified.");
    }
  }

  try {
    Envoy::MessageUtil::validate(*toCommandLineOptionsInternal(),
                                 Envoy::ProtobufMessage::getStrictValidationVisitor());
  } catch (const Envoy::ProtoValidationException& e) {
    throw MalformedArgvException(e.what());
  }
}

CommandLineOptionsPtr OptionsImpl::toCommandLineOptions() const {
  return toCommandLineOptionsInternal();
}

CommandLineOptionsPtr OptionsImpl::toCommandLineOptionsInternal() const {
  CommandLineOptionsPtr command_line_options =
      std::make_unique<nighthawk::client::CommandLineOptions>();

  command_line_options->mutable_connections()->set_value(connections_);
  if (!no_duration_) {
    command_line_options->mutable_duration()->set_seconds(duration_);
  }
  command_line_options->mutable_requests_per_second()->set_value(requests_per_second_);
  command_line_options->mutable_timeout()->set_seconds(timeout_);
  command_line_options->mutable_h2()->set_value(h2_);
  if (uri_.has_value()) {
    command_line_options->mutable_uri()->set_value(uri_.value());
  } else {
    nighthawk::client::MultiTarget* multi_target = command_line_options->mutable_multi_target();
    multi_target->mutable_path()->set_value(multi_target_path_);
    multi_target->mutable_use_https()->set_value(multi_target_use_https_);
    for (const nighthawk::client::MultiTarget::Endpoint& endpoint : multi_target_endpoints_) {
      nighthawk::client::MultiTarget::Endpoint* proto_endpoint = multi_target->add_endpoints();
      proto_endpoint->mutable_address()->set_value(endpoint.address().value());
      proto_endpoint->mutable_port()->set_value(endpoint.port().value());
    }
  }
  command_line_options->mutable_concurrency()->set_value(concurrency_);
  command_line_options->mutable_verbosity()->set_value(verbosity_);
  command_line_options->mutable_output_format()->set_value(output_format_);
  command_line_options->mutable_prefetch_connections()->set_value(prefetch_connections_);
  command_line_options->mutable_burst_size()->set_value(burst_size_);
  command_line_options->mutable_address_family()->set_value(
      static_cast<nighthawk::client::AddressFamily_AddressFamilyOptions>(address_family_));

  if (requestSource() != "") {
    auto request_source = command_line_options->mutable_request_source();
    *request_source->mutable_uri() = request_source_;
  } else if (request_source_plugin_config_.has_value()) {
    *(command_line_options->mutable_request_source_plugin_config()) =
        request_source_plugin_config_.value();
  } else {
    auto request_options = command_line_options->mutable_request_options();
    request_options->set_request_method(request_method_);
    for (const auto& header : request_headers_) {
      auto header_value_option = request_options->add_request_headers();
      // TODO(oschaaf): expose append option in CLI? For now we just set.
      header_value_option->mutable_append()->set_value(false);
      auto request_header = header_value_option->mutable_header();
      // Skip past the first colon so we propagate ':authority: foo` correctly.
      auto pos = header.empty() ? std::string::npos : header.find(':', 1);
      if (pos != std::string::npos) {
        request_header->set_key(std::string(absl::StripAsciiWhitespace(header.substr(0, pos))));
        // Any visible char, including ':', is allowed in header values.
        request_header->set_value(std::string(absl::StripAsciiWhitespace(header.substr(pos + 1))));
      } else {
        throw MalformedArgvException("A ':' is required in a header.");
      }
      request_options->mutable_request_body_size()->set_value(requestBodySize());
    }
  }

  // Only set the tls context if needed, to avoid a warning being logged about field deprecation.
  // Ideally this would follow the way transport_socket uses absl::optional below.
  // But as this field is about to get eliminated this minimal effort shortcut may be more suitable.
  if (tls_context_.ByteSizeLong() > 0) {
    *(command_line_options->mutable_tls_context()) = tls_context_;
  }
  if (transport_socket_.has_value()) {
    *(command_line_options->mutable_transport_socket()) = transport_socket_.value();
  }
  command_line_options->mutable_max_pending_requests()->set_value(max_pending_requests_);
  command_line_options->mutable_max_active_requests()->set_value(max_active_requests_);
  command_line_options->mutable_max_requests_per_connection()->set_value(
      max_requests_per_connection_);
  command_line_options->mutable_sequencer_idle_strategy()->set_value(sequencer_idle_strategy_);
  command_line_options->mutable_trace()->set_value(trace_);
  command_line_options->mutable_experimental_h1_connection_reuse_strategy()->set_value(
      experimental_h1_connection_reuse_strategy_);
  auto termination_predicates_option = command_line_options->mutable_termination_predicates();
  for (const auto& predicate : termination_predicates_) {
    termination_predicates_option->insert({predicate.first, predicate.second});
  }
  auto failure_predicates_option = command_line_options->mutable_failure_predicates();
  for (const auto& predicate : failure_predicates_) {
    failure_predicates_option->insert({predicate.first, predicate.second});
  }
  command_line_options->mutable_open_loop()->set_value(open_loop_);
  if (jitter_uniform_.count() > 0) {
    *command_line_options->mutable_jitter_uniform() =
        Envoy::Protobuf::util::TimeUtil::NanosecondsToDuration(jitter_uniform_.count());
  }
  command_line_options->mutable_nighthawk_service()->set_value(nighthawk_service_);
  command_line_options->mutable_experimental_h2_use_multiple_connections()->set_value(
      h2_use_multiple_connections_);
  for (const auto& label : labels_) {
    *command_line_options->add_labels() = label;
  }
  command_line_options->mutable_simple_warmup()->set_value(simple_warmup_);
  if (no_duration_) {
    command_line_options->mutable_no_duration()->set_value(no_duration_);
  }
  for (const envoy::config::metrics::v3::StatsSink& stats_sink : stats_sinks_) {
    *command_line_options->add_stats_sinks() = stats_sink;
  }
  command_line_options->mutable_stats_flush_interval()->set_value(stats_flush_interval_);
  command_line_options->mutable_latency_response_header_name()->set_value(
      latency_response_header_name_);
  command_line_options->mutable_allow_envoy_deprecated_v2_api()->set_value(
      allow_envoy_deprecated_v2_api_);
  if (scheduled_start_.has_value()) {
    *(command_line_options->mutable_scheduled_start()) =
        Envoy::ProtobufUtil::TimeUtil::NanosecondsToTimestamp(
            scheduled_start_.value().time_since_epoch().count());
  }
  return command_line_options;
}

} // namespace Client
} // namespace Nighthawk
