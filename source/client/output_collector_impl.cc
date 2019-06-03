#include "client/output_collector_impl.h"

#include <google/protobuf/util/time_util.h>

#include <chrono>
#include <sstream>

#include "common/protobuf/utility.h"

namespace Nighthawk {
namespace Client {

OutputCollectorImpl::OutputCollectorImpl(Envoy::TimeSource& time_source, const Options& options) {
  *(output_.mutable_timestamp()) = Envoy::Protobuf::util::TimeUtil::NanosecondsToTimestamp(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          time_source.systemTime().time_since_epoch())
          .count());
  output_.set_allocated_options(options.toCommandLineOptions().release());
}

nighthawk::client::Output OutputCollectorImpl::toProto() const { return output_; }

ConsoleOutputCollectorImpl::ConsoleOutputCollectorImpl(Envoy::TimeSource& time_source,
                                                       const Options& options)
    : OutputCollectorImpl(time_source, options) {}

std::string ConsoleOutputCollectorImpl::toString() const {
  std::stringstream ss;
  const auto& output = toProto();
  ss << "Nighthawk - A layer 7 protocol benchmarking tool." << std::endl << std::endl;
  for (const auto& result : output.results()) {
    if (result.name() == "global") {
      for (const auto& statistic : result.statistics()) {
        if (statistic.count() == 0) {
          continue;
        }
        ss << fmt::format("{}", statIdtoFriendlyStatName(statistic.id())) << std::endl;
        ss << fmt::format("  samples: {}", statistic.count()) << std::endl;
        ss << fmt::format("  mean:    {}", formatProtoDuration(statistic.mean())) << std::endl;
        ss << fmt::format("  pstdev:  {}", formatProtoDuration(statistic.pstdev())) << std::endl;
        ss << std::endl;
        ss << fmt::format("  {:<{}}{:<{}}{:<{}}", "Percentile", 12, "Count", 12, "Latency", 15)
           << std::endl;

        // The proto percentiles are ordered ascending. We write the first match to the stream.
        double last_percentile = -1.;
        for (const double p : {.0, .5, .75, .8, .9, .95, .99, .999, 1.}) {
          for (const auto& percentile : statistic.percentiles()) {
            if (percentile.percentile() >= p && last_percentile < percentile.percentile()) {
              last_percentile = percentile.percentile();
              ss << fmt::format("  {:<{}}{:<{}}{:<{}}", percentile.percentile(), 12,
                                percentile.count(), 12, formatProtoDuration(percentile.duration()),
                                15)
                 << std::endl;
              break;
            }
          }
        }
        ss << std::endl;
      }
      ss << fmt::format("{:<{}}{:<{}}{}", "Counter", 40, "Value", 12, "Per second") << std::endl;
      for (const auto& counter : result.counters()) {
        ss << fmt::format("{:<{}}{:<{}}{:.{}f}", counter.name(), 40, counter.value(), 12,
                          counter.value() / (output.options().duration().seconds() * 1.0), 2)
           << std::endl;
      }
      ss << std::endl;
    }
  }

  return ss.str();
}

std::string
ConsoleOutputCollectorImpl::formatProtoDuration(const Envoy::Protobuf::Duration& duration) const {
  auto c = Envoy::ProtobufUtil::TimeUtil::DurationToMicroseconds(duration);
  return fmt::format("{}s {:03}ms {:03}us", (c % 1'000'000'000) / 1'000'000,
                     (c % 1'000'000) / 1'000, c % 1'000);
}

std::string ConsoleOutputCollectorImpl::statIdtoFriendlyStatName(absl::string_view stat_id) const {
  if (stat_id == "benchmark_http_client.queue_to_connect") {
    return "Queueing and connection setup latency";
  } else if (stat_id == "benchmark_http_client.request_to_response") {
    return "Request start to response end";
  } else if (stat_id == "sequencer.callback") {
    return "Initiation to completion";
  } else if (stat_id == "sequencer.blocking") {
    return "Blocking. Results are skewed when significant numbers are reported here.";
  }
  return std::string(stat_id);
}

void OutputCollectorImpl::addResult(absl::string_view name,
                                    const std::vector<StatisticPtr>& statistics,
                                    const std::map<std::string, uint64_t>& counters) {
  auto result = output_.add_results();
  result->set_name(name.data(), name.size());
  for (auto& statistic : statistics) {
    *(result->add_statistics()) = statistic->toProto();
  }
  for (const auto& counter : counters) {
    auto new_counters = result->add_counters();
    new_counters->set_name(counter.first);
    new_counters->set_value(counter.second);
  }
}

JsonOutputCollectorImpl::JsonOutputCollectorImpl(Envoy::TimeSource& time_source,
                                                 const Options& options)
    : OutputCollectorImpl(time_source, options) {}

std::string JsonOutputCollectorImpl::toString() const {
  return Envoy::MessageUtil::getJsonStringFromMessage(toProto(), true, true);
}

YamlOutputCollectorImpl::YamlOutputCollectorImpl(Envoy::TimeSource& time_source,
                                                 const Options& options)
    : OutputCollectorImpl(time_source, options) {}

std::string YamlOutputCollectorImpl::toString() const {
  return Envoy::MessageUtil::getYamlStringFromMessage(toProto(), true, true);
}

} // namespace Client
} // namespace Nighthawk