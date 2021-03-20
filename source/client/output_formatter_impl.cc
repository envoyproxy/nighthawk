#include "client/output_formatter_impl.h"

#include <google/protobuf/util/time_util.h>

#include <chrono>
#include <regex>
#include <sstream>

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/protobuf/utility.h"

#include "api/client/transform/fortio.pb.h"

#include "common/version_info.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"

namespace Nighthawk {
namespace Client {

std::vector<std::string> OutputFormatterImpl::getLowerCaseOutputFormats() {
  const Envoy::Protobuf::EnumDescriptor* enum_descriptor =
      nighthawk::client::OutputFormat::OutputFormatOptions_descriptor();
  std::vector<std::string> values;
  // We skip the first, which is DEFAULT, as it's not selectable.
  for (int i = 1; i < enum_descriptor->value_count(); ++i) {
    auto* value_descriptor = enum_descriptor->value(i);
    std::string name = value_descriptor->name();
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    values.push_back(name);
  }
  return values;
}

void OutputFormatterImpl::iteratePercentiles(
    const nighthawk::client::Statistic& statistic,
    const std::function<void(const nighthawk::client::Percentile&)>& callback) const {
  // The proto percentiles are ordered ascending. We write the first match to the stream.
  double last_percentile = -1.;
  for (const double p : {.0, .5, .75, .8, .9, .95, .99, .999, 1.}) {
    for (const auto& percentile : statistic.percentiles()) {
      if (percentile.percentile() >= p && last_percentile < percentile.percentile()) {
        last_percentile = percentile.percentile();
        callback(percentile);
        break;
      }
    }
  }
}

std::string ConsoleOutputFormatterImpl::formatProto(const nighthawk::client::Output& output) const {
  std::stringstream ss;
  ss << "Nighthawk - A layer 7 protocol benchmarking tool." << std::endl << std::endl;
  for (const auto& result : output.results()) {
    if (result.name() == "global") {
      for (const auto& statistic : result.statistics()) {
        // Don't show output for statistics that have no samples.
        if (statistic.count() == 0) {
          continue;
        }
        const std::string s_min = statistic.has_min() ? formatProtoDuration(statistic.min())
                                                      : fmt::format("{}", statistic.raw_min());
        const std::string s_max = statistic.has_max() ? formatProtoDuration(statistic.max())
                                                      : fmt::format("{}", statistic.raw_max());
        const std::string s_mean = statistic.has_mean() ? formatProtoDuration(statistic.mean())
                                                        : fmt::format("{}", statistic.raw_mean());
        const std::string s_pstdev = statistic.has_pstdev()
                                         ? formatProtoDuration(statistic.pstdev())
                                         : fmt::format("{}", statistic.raw_pstdev());

        ss << fmt::format("{} ({} samples)", statIdtoFriendlyStatName(statistic.id()),
                          statistic.count())
           << std::endl;
        ss << fmt::format("  min: {} | ", s_min);
        ss << fmt::format("mean: {} | ", s_mean);
        ss << fmt::format("max: {} | ", s_max);
        ss << fmt::format("pstdev: {}", s_pstdev) << std::endl;

        bool header_written = false;
        iteratePercentiles(statistic, [&ss, this, &header_written](
                                          const nighthawk::client::Percentile& percentile) {
          const auto p = percentile.percentile();
          // Don't show the min / max, as we already show that above.
          if (p > 0 && p < 1) {
            if (!header_written) {
              ss << std::endl
                 << fmt::format("  {:<{}}{:<{}}{:<{}}", "Percentile", 12, "Count", 12, "Value", 15)
                 << std::endl;
              header_written = true;
            }
            auto s_percentile = fmt::format("{:.{}g}", p, 8);
            ss << fmt::format("  {:<{}}{:<{}}{:<{}}", s_percentile, 12, percentile.count(), 12,
                              percentile.has_duration()
                                  ? formatProtoDuration(percentile.duration())
                                  : fmt::format("{}", static_cast<int64_t>(percentile.raw_value())),
                              15)
               << std::endl;
          }
        });
        ss << std::endl;
      }
      ss << fmt::format("{:<{}}{:<{}}{}", "Counter", 40, "Value", 12, "Per second") << std::endl;
      for (const auto& counter : result.counters()) {
        const auto nanos =
            Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(result.execution_duration());
        ss << fmt::format("{:<{}}{:<{}}{:.{}f}", counter.name(), 40, counter.value(), 12,
                          counter.value() / (nanos / 1e9), 2)
           << std::endl;
      }
      ss << std::endl;
    }
  }

  return ss.str();
}

std::string ConsoleOutputFormatterImpl::formatProtoDuration(
    const Envoy::ProtobufWkt::Duration& duration) const {
  auto c = Envoy::Protobuf::util::TimeUtil::DurationToMicroseconds(duration);
  return fmt::format("{}s {:03}ms {:03}us", (c % 1'000'000'000) / 1'000'000,
                     (c % 1'000'000) / 1'000, c % 1'000);
}

std::string ConsoleOutputFormatterImpl::statIdtoFriendlyStatName(absl::string_view stat_id) {
  if (stat_id == "benchmark_http_client.queue_to_connect") {
    return "Queueing and connection setup latency";
  } else if (stat_id == "benchmark_http_client.request_to_response") {
    return "Request start to response end";
  } else if (stat_id == "sequencer.callback") {
    return "Initiation to completion";
  } else if (stat_id == "sequencer.blocking") {
    return "Blocking. Results are skewed when significant numbers are reported here.";
  } else if (stat_id == "benchmark_http_client.response_body_size") {
    return "Response body size in bytes";
  } else if (stat_id == "benchmark_http_client.response_header_size") {
    return "Response header size in bytes";
  }

  return std::string(stat_id);
}

std::string JsonOutputFormatterImpl::formatProto(const nighthawk::client::Output& output) const {
  return Envoy::MessageUtil::getJsonStringFromMessageOrDie(output, true, true);
}

std::string YamlOutputFormatterImpl::formatProto(const nighthawk::client::Output& output) const {
  return Envoy::MessageUtil::getYamlStringFromMessage(output, true, true);
}

std::string
DottedStringOutputFormatterImpl::formatProto(const nighthawk::client::Output& output) const {
  std::stringstream ss;
  for (const auto& result : output.results()) {
    for (const auto& statistic : result.statistics()) {
      const std::string prefix = fmt::format("{}.{}", result.name(), statistic.id());
      const std::string s_min =
          statistic.has_min()
              ? fmt::format(
                    "{}", Envoy::Protobuf::util::TimeUtil::DurationToMicroseconds(statistic.min()))
              : fmt::format("{}", statistic.raw_min());
      const std::string s_max =
          statistic.has_max()
              ? fmt::format(
                    "{}", Envoy::Protobuf::util::TimeUtil::DurationToMicroseconds(statistic.max()))
              : fmt::format("{}", statistic.raw_max());
      const std::string s_mean =
          statistic.has_mean()
              ? fmt::format(
                    "{}", Envoy::Protobuf::util::TimeUtil::DurationToMicroseconds(statistic.mean()))
              : fmt::format("{}", statistic.raw_mean());
      const std::string s_pstdev =
          statistic.has_pstdev()
              ? fmt::format("{}", Envoy::Protobuf::util::TimeUtil::DurationToMicroseconds(
                                      statistic.pstdev()))
              : fmt::format("{}", statistic.raw_pstdev());

      ss << fmt::format("{}.samples: {}", prefix, statistic.count()) << std::endl;
      ss << fmt::format("{}.mean: {}", prefix, s_mean) << std::endl;
      ss << fmt::format("{}.pstdev: {}", prefix, s_pstdev) << std::endl;
      ss << fmt::format("{}.min: {}", prefix, s_min) << std::endl;
      ss << fmt::format("{}.max: {}", prefix, s_max) << std::endl;

      iteratePercentiles(statistic, [&ss, prefix](const nighthawk::client::Percentile& percentile) {
        const std::string percentile_prefix =
            fmt::format("{}.permilles-{:.{}f}", prefix, percentile.percentile() * 1000, 0);
        ss << fmt::format("{}.count: {}", percentile_prefix, percentile.count()) << std::endl;
        if (percentile.has_duration()) {
          ss << fmt::format(
              "{}.microseconds: {}", percentile_prefix,
              Envoy::Protobuf::util::TimeUtil::DurationToMicroseconds(percentile.duration()));
        } else {
          ss << fmt::format("{}.value: {}", percentile_prefix,
                            static_cast<int64_t>(percentile.raw_value()));
        }
        ss << std::endl;
      });
    }
    for (const auto& counter : result.counters()) {
      const std::string prefix = fmt::format("{}.{}", result.name(), counter.name());
      ss << fmt::format("{}:{}", prefix, counter.value()) << std::endl;
    }
  }
  return ss.str();
}

const nighthawk::client::Result&
FortioOutputFormatterImpl::getGlobalResult(const nighthawk::client::Output& output) const {
  for (const auto& nh_result : output.results()) {
    if (nh_result.name() == "global") {
      return nh_result;
    }
  }

  throw NighthawkException("Nighthawk output was malformed, contains no 'global' results.");
}

uint64_t FortioOutputFormatterImpl::getCounterValue(const nighthawk::client::Result& result,
                                                    absl::string_view counter_name,
                                                    const uint64_t value_if_not_found) const {
  for (const auto& nh_counter : result.counters()) {
    if (nh_counter.name() == counter_name) {
      return nh_counter.value();
    }
  }
  return value_if_not_found;
}

const nighthawk::client::Statistic*
FortioOutputFormatterImpl::findStatistic(const nighthawk::client::Result& result,
                                         absl::string_view stat_id) const {
  for (auto const& nh_stat : result.statistics()) {
    if (nh_stat.id() == stat_id) {
      return &nh_stat;
    }
  }
  return nullptr;
}

std::chrono::nanoseconds FortioOutputFormatterImpl::getAverageExecutionDuration(
    const nighthawk::client::Output& output) const {
  if (!output.results_size()) {
    throw NighthawkException("No results in output");
  }
  const auto& r = output.results().at(output.results_size() - 1);
  ASSERT(r.name() == "global");
  return std::chrono::nanoseconds(
      Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(r.execution_duration()));
}

double
FortioOutputFormatterImpl::durationToSeconds(const Envoy::ProtobufWkt::Duration& duration) const {
  return Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(duration) / 1e9;
}

std::string FortioOutputFormatterImpl::formatProto(const nighthawk::client::Output& output) const {
  nighthawk::client::FortioResult fortio_output;
  // Iff there's only a single worker we will have only a single result. Otherwise the number of
  // workers can be derived by substracting one from the number of results (for the
  // aggregated/global result).
  const uint32_t number_of_workers = output.results().size() == 1 ? 1 : output.results().size() - 1;
  std::string labels;
  for (const auto& label : output.options().labels()) {
    labels += label + " ";
  }
  fortio_output.set_labels(std::string(absl::StripSuffix(labels, " ")));
  fortio_output.set_version(VersionInfo::toVersionString(output.version()));
  *fortio_output.mutable_starttime() = output.timestamp();
  fortio_output.set_requestedqps(number_of_workers *
                                 output.options().requests_per_second().value());
  fortio_output.set_url(output.options().uri().value());
  *fortio_output.mutable_requestedduration() = output.options().duration();
  auto actual_duration = getAverageExecutionDuration(output);
  fortio_output.set_actualduration(actual_duration.count());
  fortio_output.set_jitter(output.options().has_jitter_uniform() &&
                           (output.options().jitter_uniform().nanos() > 0 ||
                            output.options().jitter_uniform().seconds() > 0));
  fortio_output.set_runtype("HTTP");

  // The stock Envoy h2 pool doesn't offer support for multiple connections here. So we must ignore
  // the connections setting when h2 is enabled and the experimental h2-pool which supports multiple
  // connections isn't enabled. Also, the number of workers acts as a multiplier.
  const uint32_t number_of_connections =
      ((output.options().h2().value() &&
        !output.options().experimental_h2_use_multiple_connections().value())
           ? 1
           : output.options().connections().value()) *
      number_of_workers;
  // This displays as "connections" in the UI, not threads.
  fortio_output.set_numthreads(number_of_connections);

  // Get the result that represents all workers (global)
  const auto& nh_global_result = getGlobalResult(output);

  // Fill in the actual QPS based on the counters
  const double actual_qps =
      static_cast<double>(getCounterValue(nh_global_result, "upstream_rq_total", 0) /
                          std::chrono::duration<double>(actual_duration).count());
  fortio_output.set_actualqps(actual_qps);
  fortio_output.set_bytesreceived(
      getCounterValue(nh_global_result, "upstream_cx_rx_bytes_total", 0));
  fortio_output.set_bytessent(getCounterValue(nh_global_result, "upstream_cx_tx_bytes_total", 0));
  // Fortio-ui only reads the 200 OK field, other fields are never displayed.
  // Fortio computes the error percentage based on:
  // - the sample count in the histogram
  // - the number of 200 responses
  fortio_output.mutable_retcodes()->insert(
      {"200", getCounterValue(nh_global_result, "benchmark.http_2xx", 0)});
  auto* statistic = findStatistic(nh_global_result, "benchmark_http_client.request_to_response");
  if (statistic != nullptr) {
    fortio_output.mutable_durationhistogram()->CopyFrom(renderFortioDurationHistogram(*statistic));
  }
  statistic = findStatistic(nh_global_result, "benchmark_http_client.response_body_size");
  if (statistic != nullptr) {
    fortio_output.mutable_sizes()->CopyFrom(renderFortioDurationHistogram(*statistic));
  }
  statistic = findStatistic(nh_global_result, "benchmark_http_client.response_header_size");
  if (statistic != nullptr) {
    fortio_output.mutable_headersizes()->CopyFrom(renderFortioDurationHistogram(*statistic));
  }
  return Envoy::MessageUtil::getJsonStringFromMessageOrDie(fortio_output, true, true);
}

const nighthawk::client::DurationHistogram FortioOutputFormatterImpl::renderFortioDurationHistogram(
    const nighthawk::client::Statistic& nh_stat) const {
  nighthawk::client::DurationHistogram fortio_histogram;
  uint64_t prev_fortio_count = 0;
  double prev_fortio_end = 0;
  int i = 0;
  for (const auto& nh_percentile : nh_stat.percentiles()) {
    nighthawk::client::DataEntry fortio_data_entry;
    // fortio_percent = 100 * nh_percentile
    fortio_data_entry.set_percent(nh_percentile.percentile() * 100);

    // fortio_count = nh_count - prev_fortio_count
    fortio_data_entry.set_count(nh_percentile.count() - prev_fortio_count);

    // fortio_end = nh_duration (need to convert formats)
    double value;
    if (nh_percentile.has_duration()) {
      // fortio_end = nh_duration (need to convert formats)
      value = durationToSeconds(nh_percentile.duration());
    } else {
      value = nh_percentile.raw_value();
    }
    fortio_data_entry.set_end(value);

    // fortio_start = prev_fortio_end
    if (i++ == 0) {
      // If this is the first entry, force the start and end time to be the same.
      // This prevents it from starting at 0, making it disproportionally big in the UI.
      prev_fortio_end = value;
    }
    fortio_data_entry.set_start(prev_fortio_end);

    // Update tracking variables
    prev_fortio_count = nh_percentile.count();
    prev_fortio_end = value;

    // Set the data entry in the histogram only if it's not the first entry
    fortio_histogram.add_data()->CopyFrom(fortio_data_entry);
  }

  // Set the count (number of data points)
  fortio_histogram.set_count(nh_stat.count());
  fortio_histogram.set_avg(nh_stat.has_mean() ? durationToSeconds(nh_stat.mean())
                                              : nh_stat.raw_mean());
  fortio_histogram.set_min(nh_stat.has_min() ? durationToSeconds(nh_stat.min())
                                             : nh_stat.raw_min());
  fortio_histogram.set_sum(nh_stat.count() * fortio_histogram.avg());
  fortio_histogram.set_max(nh_stat.has_max() ? durationToSeconds(nh_stat.max())
                                             : nh_stat.raw_max());
  // Note that Nighthawk tracks pstdev whereas fortio seems to use stdev.
  fortio_histogram.set_stddev(nh_stat.has_pstdev() ? durationToSeconds(nh_stat.pstdev())
                                                   : nh_stat.raw_pstdev());
  iteratePercentiles(nh_stat,
                     [this, &fortio_histogram](const nighthawk::client::Percentile& percentile) {
                       if (percentile.percentile() > 0 && percentile.percentile() < 1) {
                         auto* p = fortio_histogram.add_percentiles();
                         // We perform some rounding on the percentiles for a better UX while we use
                         // HdrHistogram. HDR-Histogram uses base-2 arithmetic behind the scenes
                         // which yields percentiles close to what fortio has, but not perfectly
                         // on-spot, e.g. 0.990625 and 0.9990234375.
                         p->set_percentile(std::floor(percentile.percentile() * 1000) / 10);
                         if (percentile.has_duration()) {
                           p->set_value(durationToSeconds(percentile.duration()));
                         } else {
                           p->set_value(percentile.raw_value());
                         }
                       }
                     });
  return fortio_histogram;
}

std::string
FortioPedanticOutputFormatterImpl::formatProto(const nighthawk::client::Output& output) const {
  std::string res = FortioOutputFormatterImpl::formatProto(output);
  // clang-format off
  // Fix two types of quirks. We disable linting because we use std::regex directly.
  // This should be OK as the regular expression we use can be trusted.
  // 1. We misdefined RequestedRPS as an int, whereas Fortio outputs that as a string.
  res = std::regex_replace(res, std::regex(R"EOF("RequestedQPS"\: ([0-9]*))EOF"),
                           R"EOF("RequestedQPS": "$1")EOF");
  // 2. Our uint64's get serialized as json strings. Fortio outputs them as json integers.
  // An example of a string that would match the regular expression below would be:
  // "Count": "100", which then would be replaced to look like: "Count": 100.
  // NOTE: [0-9][0-9][0-9] looks for string fields referring to http status codes, which get counted.
 res = std::regex_replace(
      res, std::regex(R"EOF("([0-9][0-9][0-9]|Count|BytesSent|BytesReceived)"\: "([0-9]*)")EOF"),
      R"EOF("$1": $2)EOF");
  // clang-format on
  return res;
}

} // namespace Client
} // namespace Nighthawk
