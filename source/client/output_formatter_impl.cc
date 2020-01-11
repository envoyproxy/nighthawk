#include "client/output_formatter_impl.h"

#include <google/protobuf/util/time_util.h>

#include <chrono>
#include <sstream>

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/protobuf/utility.h"

#include "api/client/transform/fortio.pb.h"

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
        const bool duration_valued =
            statistic.domain() == nighthawk::client::Statistic_StatisticDomain_DURATION;
        const std::string s_min = duration_valued ? formatProtoDuration(statistic.min())
                                                  : fmt::format("{}", statistic.raw_min());
        const std::string s_max = duration_valued ? formatProtoDuration(statistic.max())
                                                  : fmt::format("{}", statistic.raw_max());
        const std::string s_mean = duration_valued ? formatProtoDuration(statistic.mean())
                                                   : fmt::format("{}", statistic.raw_mean());
        const std::string s_pstdev = duration_valued ? formatProtoDuration(statistic.pstdev())
                                                     : fmt::format("{}", statistic.raw_pstdev());

        ss << fmt::format("{} ({} samples)", statIdtoFriendlyStatName(statistic.id()),
                          statistic.count())
           << std::endl;
        ss << fmt::format("  min: {} | ", s_min);
        ss << fmt::format("mean: {} | ", s_mean);
        ss << fmt::format("max: {} | ", s_max);
        ss << fmt::format("pstdev: {}", s_pstdev) << std::endl;

        bool header_written = false;
        iteratePercentiles(statistic, [&ss, this, duration_valued, &header_written](
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
            ss << fmt::format("  {:<{}}{:<{}}{:<{}}", p, 12, percentile.count(), 12,
                              duration_valued ? formatProtoDuration(percentile.duration())
                                              : fmt::format("{}", percentile.raw_value()),
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
  return Envoy::MessageUtil::getJsonStringFromMessage(output, true, true);
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
      const bool duration_valued =
          statistic.domain() == nighthawk::client::Statistic_StatisticDomain_DURATION;
      const std::string s_min =
          duration_valued
              ? fmt::format(
                    "{}", Envoy::Protobuf::util::TimeUtil::DurationToMicroseconds(statistic.min()))
              : fmt::format("{}", statistic.raw_min());
      const std::string s_max =
          duration_valued
              ? fmt::format(
                    "{}", Envoy::Protobuf::util::TimeUtil::DurationToMicroseconds(statistic.max()))
              : fmt::format("{}", statistic.raw_max());
      const std::string s_mean =
          duration_valued
              ? fmt::format(
                    "{}", Envoy::Protobuf::util::TimeUtil::DurationToMicroseconds(statistic.mean()))
              : fmt::format("{}", statistic.raw_mean());
      const std::string s_pstdev =
          duration_valued
              ? fmt::format("{}", Envoy::Protobuf::util::TimeUtil::DurationToMicroseconds(
                                      statistic.pstdev()))
              : fmt::format("{}", statistic.raw_pstdev());

      ss << fmt::format("{}.samples: {}", prefix, statistic.count()) << std::endl;
      ss << fmt::format("{}.mean: {}", prefix, s_mean) << std::endl;
      ss << fmt::format("{}.pstdev: {}", prefix, s_pstdev) << std::endl;
      ss << fmt::format("{}.min: {}", prefix, s_min) << std::endl;
      ss << fmt::format("{}.max: {}", prefix, s_max) << std::endl;

      iteratePercentiles(statistic, [&ss, prefix, duration_valued](
                                        const nighthawk::client::Percentile& percentile) {
        const std::string percentile_prefix =
            fmt::format("{}.permilles-{:.{}f}", prefix, percentile.percentile() * 1000, 0);
        ss << fmt::format("{}.count: {}", percentile_prefix, percentile.count()) << std::endl;
        if (duration_valued) {
          ss << fmt::format(
              "{}.microseconds: {}", percentile_prefix,
              Envoy::Protobuf::util::TimeUtil::DurationToMicroseconds(percentile.duration()));
        } else {
          ss << fmt::format("{}.value: {}", percentile_prefix, percentile.raw_value());
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

const nighthawk::client::Counter&
FortioOutputFormatterImpl::getCounterByName(const nighthawk::client::Result& result,
                                            absl::string_view counter_name) const {
  for (const auto& nh_counter : result.counters()) {
    if (nh_counter.name() == counter_name) {
      return nh_counter;
    }
  }

  throw NighthawkException(absl::StrCat(
      "Nighthawk result was malformed, contains no counter with name: ", counter_name));
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

double
FortioOutputFormatterImpl::durationToSeconds(const Envoy::ProtobufWkt::Duration& duration) const {
  return Envoy::Protobuf::util::TimeUtil::DurationToNanoseconds(duration) / 1e9;
}

std::string FortioOutputFormatterImpl::formatProto(const nighthawk::client::Output& output) const {
  nighthawk::client::FortioResult fortio_output;
  std::string labels;
  for (const auto& label : output.options().labels()) {
    labels += label + " ";
  }
  fortio_output.set_labels(std::string(absl::StripSuffix(labels, " ")));
  fortio_output.mutable_starttime()->set_seconds(output.timestamp().seconds());
  fortio_output.set_requestedqps(output.options().requests_per_second().value());
  fortio_output.set_url(output.options().uri().value());

  // Actual and requested durations are the same
  const auto& nh_duration = output.options().duration().seconds();
  fortio_output.mutable_requestedduration()->set_seconds(nh_duration);
  fortio_output.set_actualduration(nh_duration);

  // This displays as "connections" in the UI, not threads.
  // TODO(#186): This field may not be accurate for for HTTP2 load tests.
  fortio_output.set_numthreads(output.options().connections().value());

  // Get the result that represents all workers (global)
  const auto& nh_global_result = this->getGlobalResult(output);

  // Fill in the actual QPS based on the counters
  const auto& nh_rq_counter = this->getCounterByName(nh_global_result, "upstream_rq_total");
  const double actual_qps = static_cast<double>(nh_rq_counter.value()) / nh_duration;
  fortio_output.set_actualqps(actual_qps);

  // Fill in the number of successful responses.
  // Fortio-ui only reads the 200 OK field, other fields are never displayed.
  fortio_output.mutable_retcodes()->insert({"200", 0});
  try {
    const auto& nh_2xx_counter = this->getCounterByName(nh_global_result, "benchmark.http_2xx");
    fortio_output.mutable_retcodes()->at("200") = nh_2xx_counter.value();
  } catch (const NighthawkException& e) {
    // If this field doesn't exist, then there were no 2xx responses
    fortio_output.mutable_retcodes()->at("200") = 0;
  }

  auto* statistic =
      this->findStatistic(nh_global_result, "benchmark_http_client.request_to_response");
  if (statistic == nullptr) {
    throw NighthawkException("Nighthawk result was malformed, contains no "
                             "'benchmark_http_client.request_to_response' statistic.");
  }
  fortio_output.mutable_durationhistogram()->CopyFrom(renderFortioDurationHistogram(*statistic));
  statistic = this->findStatistic(nh_global_result, "benchmark_http_client.response_body_size");
  if (statistic != nullptr) {
    fortio_output.mutable_sizes()->CopyFrom(renderFortioDurationHistogram(*statistic));
  }
  statistic = this->findStatistic(nh_global_result, "benchmark_http_client.response_header_size");
  if (statistic != nullptr) {
    fortio_output.mutable_headersizes()->CopyFrom(renderFortioDurationHistogram(*statistic));
  }
  return Envoy::MessageUtil::getJsonStringFromMessage(fortio_output, true, true);
}

const nighthawk::client::DurationHistogram FortioOutputFormatterImpl::renderFortioDurationHistogram(
    const nighthawk::client::Statistic& nh_stat) const {
  nighthawk::client::DurationHistogram fortio_histogram;
  uint64_t prev_fortio_count = 0;
  double prev_fortio_end = 0;
  const int percentiles_size = nh_stat.percentiles().size();
  const bool duration_valued =
      nh_stat.domain() == nighthawk::client::Statistic_StatisticDomain_DURATION;
  for (int i = 0; i < percentiles_size; i++) {
    nighthawk::client::DataEntry fortio_data_entry;
    const auto& nh_percentile = nh_stat.percentiles().at(i);
    // fortio_percent = 100 * nh_percentile
    fortio_data_entry.set_percent(nh_percentile.percentile() * 100);

    // fortio_count = nh_count - prev_fortio_count
    fortio_data_entry.set_count(nh_percentile.count() - prev_fortio_count);

    // fortio_end = nh_duration (need to convert formats)
    double value;
    if (duration_valued) {
      // fortio_end = nh_duration (need to convert formats)
      value = durationToSeconds(nh_percentile.duration());
    } else {
      value = nh_percentile.raw_value();
    }
    fortio_data_entry.set_end(value);

    // fortio_start = prev_fortio_end
    if (i == 0) {
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
  fortio_histogram.set_avg(duration_valued ? durationToSeconds(nh_stat.mean())
                                           : nh_stat.raw_mean());
  fortio_histogram.set_min(duration_valued ? durationToSeconds(nh_stat.min()) : nh_stat.raw_min());
  fortio_histogram.set_sum(nh_stat.count() * fortio_histogram.avg());
  fortio_histogram.set_max(duration_valued ? durationToSeconds(nh_stat.max()) : nh_stat.raw_max());
  // Note that Nighthawk tracks pstdev whereas fortio seems to use stdev.
  fortio_histogram.set_stddev(duration_valued ? durationToSeconds(nh_stat.pstdev())
                                              : nh_stat.raw_pstdev());
  iteratePercentiles(nh_stat, [this, &fortio_histogram,
                               &duration_valued](const nighthawk::client::Percentile& percentile) {
    if (percentile.percentile() > 0 && percentile.percentile() < 1) {
      auto* p = fortio_histogram.add_percentiles();
      // We perform some rounding on the percentiles for a better UX while we use
      // HdrHistogram. HDR-Histogram uses base-2 arithmetic behind the scenes
      // which yields percentiles close to what fortio has, but not perfectly
      // on-spot, e.g. 0.990625 and 0.9990234375.
      p->set_percentile(std::floor(percentile.percentile() * 1000) / 10);
      if (duration_valued) {
        p->set_value(durationToSeconds(percentile.duration()));
      } else {
        p->set_value(percentile.raw_value());
      }
    }
  });
  return fortio_histogram;
}

} // namespace Client
} // namespace Nighthawk