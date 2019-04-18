#include "client/options_impl.h"

#include <cmath>

#include "tclap/CmdLine.h"

#include "common/uri_impl.h"
#include "common/utility.h"

namespace Nighthawk {
namespace Client {

OptionsImpl::OptionsImpl(int argc, const char* const* argv) {
  const char* descr = "Nighthawk, a L7 HTTP protocol family benchmarking tool based on Envoy.";
  TCLAP::CmdLine cmd(descr, ' ', "PoC"); // NOLINT

  TCLAP::ValueArg<uint64_t> requests_per_second("", "rps",
                                                "The target requests-per-second rate. Default: 5.",
                                                false, 5 /*default qps*/, "uint64_t", cmd);
  TCLAP::ValueArg<uint64_t> connections(
      "", "connections",
      "The number of connections per event loop that the test should maximally "
      "use. HTTP/1 only. Default: 1.",
      false, 1, "uint64_t", cmd);
  TCLAP::ValueArg<uint64_t> duration("", "duration",
                                     "The number of seconds that the test should run. Default: 5.",
                                     false, 5, "uint64_t", cmd);
  TCLAP::ValueArg<uint64_t> timeout(
      "", "timeout",
      "Timeout period in seconds used for both connection timeout and grace period waiting for "
      "lagging responses to come in after the test run is done. Default: 5.",
      false, 5, "uint64_t", cmd);

  TCLAP::SwitchArg h2("", "h2", "Use HTTP/2", cmd);

  TCLAP::ValueArg<std::string> concurrency(
      "", "concurrency",
      "The number of concurrent event loops that should be used. Specify 'auto' to let "
      "Nighthawk leverage all vCPUs that have affinity to the Nighthawk process.Note that "
      "increasing this results in an effective load multiplier combined with the configured-- rps "
      "and --connections values.Default : 1. ",
      false, "1", "string", cmd);

  std::vector<std::string> log_levels = {"trace", "debug", "info", "warn", "error", "critical"};
  TCLAP::ValuesConstraint<std::string> verbosities_allowed(log_levels);

  TCLAP::ValueArg<std::string> verbosity(
      "v", "verbosity",
      "Verbosity of the output. Possible values: [trace, debug, info, warn, error, critical]. The "
      "default level is 'info'.",
      false, "warn", &verbosities_allowed, cmd);

  std::vector<std::string> output_formats = {"human", "yaml", "json"};
  TCLAP::ValuesConstraint<std::string> output_formats_allowed(output_formats);

  TCLAP::ValueArg<std::string> output_format(
      "", "output-format",
      "Verbosity of the output. Possible values: [human, yaml, json]. The "
      "default output format is 'human'.",
      false, "human", &output_formats_allowed, cmd);

  TCLAP::SwitchArg prefetch_connections(
      "", "prefetch-connections", "Prefetch connections before benchmarking (HTTP/1 only).", cmd);

  // Note: we allow a burst size of 1, which intuitively may not make sense. However, allowing it
  // doesn't hurt either, and it does allow one to use a the same code-execution-paths in test
  // series that ramp up burst sizes.
  TCLAP::ValueArg<uint64_t> burst_size(
      "", "burst-size",
      "Release requests in bursts of the specified size (default: 0, no bursting).", false, 0,
      "uint64_t", cmd);

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

  requests_per_second_ = requests_per_second.getValue();
  connections_ = connections.getValue();
  duration_ = duration.getValue();
  timeout_ = timeout.getValue();
  uri_ = uri.getValue();
  h2_ = h2.getValue();
  concurrency_ = concurrency.getValue();
  verbosity_ = verbosity.getValue();
  output_format_ = output_format.getValue();
  prefetch_connections_ = prefetch_connections.getValue();
  burst_size_ = burst_size.getValue();

  // We cap on negative values. TCLAP accepts negative values which we will get here as very
  // large values. We just cap values to 2^63.
  const uint64_t largest_acceptable_uint64_option_value =
      static_cast<uint64_t>(std::pow(2ull, 63ull));

  if (requests_per_second_ == 0 || requests_per_second_ > largest_acceptable_uint64_option_value) {
    throw MalformedArgvException("Invalid value for --rps");
  }
  if (connections_ == 0 || connections_ > largest_acceptable_uint64_option_value) {
    throw MalformedArgvException("Invalid value for --connections");
  }
  if (duration_ == 0 || duration_ > largest_acceptable_uint64_option_value) {
    throw MalformedArgvException("Invalid value for --duration");
  }
  if (timeout_ == 0 || timeout_ > largest_acceptable_uint64_option_value) {
    throw MalformedArgvException("Invalid value for --timeout");
  }

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
}

CommandLineOptionsPtr OptionsImpl::toCommandLineOptions() const {
  CommandLineOptionsPtr command_line_options =
      std::make_unique<nighthawk::client::CommandLineOptions>();

  command_line_options->set_connections(connections());
  command_line_options->mutable_duration()->set_seconds(duration().count());
  command_line_options->set_requests_per_second(requestsPerSecond());
  command_line_options->mutable_timeout()->set_seconds(timeout().count());
  command_line_options->set_h2(h2());
  command_line_options->set_uri(uri());
  command_line_options->set_concurrency(concurrency());
  command_line_options->set_verbosity(verbosity());
  command_line_options->set_output_format(outputFormat());
  command_line_options->set_prefetch_connections(prefetchConnections());
  command_line_options->set_burst_size(burstSize());

  return command_line_options;
}

} // namespace Client
} // namespace Nighthawk
