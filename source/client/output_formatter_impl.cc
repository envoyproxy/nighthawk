#include "client/output_formatter_impl.h"

#include <chrono>
#include <sstream>

#include "common/protobuf/utility.h"

namespace Nighthawk {
namespace Client {

OutputFormatterImpl::OutputFormatterImpl(Envoy::TimeSource& time_source, const Options& options) {
  *(output_.mutable_timestamp()) = Envoy::Protobuf::util::TimeUtil::NanosecondsToTimestamp(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          time_source.systemTime().time_since_epoch())
          .count());
  output_.set_allocated_options(options.toCommandLineOptions().release());
}

nighthawk::client::Output OutputFormatterImpl::toProto() const { return output_; }

ConsoleOutputFormatterImpl::ConsoleOutputFormatterImpl(Envoy::TimeSource& time_source,
                                                       const Options& options)
    : OutputFormatterImpl(time_source, options) {}

std::string ConsoleOutputFormatterImpl::toString() const {
  std::stringstream ss;
  const auto& output = toProto();
  ss << "Nighthawk - A layer 7 protocol benchmarking tool." << std::endl << std::endl;
  for (const auto& result : output.results()) {
    if (result.name() == "global") {
      for (const auto& statistic : result.statistics()) {
        if (statistic.count() == 0) {
          continue;
        }
        ss << fmt::format("{}: {} samples, mean: {}, pstdev: {}", statistic.id(), statistic.count(),
                          statistic.mean(), statistic.pstdev())
           << std::endl;
        ss << fmt::format("{:<{}}{:<{}}{:<{}}", "Percentile", 12, "Count", 12, "Latency", 15)
           << std::endl;

        // The proto percentiles are ordered ascending. We write the first match to the stream.
        double last_percentile = -1.;
        for (const double p : {.0, .5, .75, .8, .9, .95, .99, .999, 1.}) {
          for (const auto& percentile : statistic.percentiles()) {
            if (percentile.percentile() >= p && last_percentile < percentile.percentile()) {
              last_percentile = percentile.percentile();
              ss << fmt::format("{:<{}}{:<{}}{:<{}}", percentile.percentile(), 12,
                                percentile.count(), 12, percentile.duration(), 15)
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

void OutputFormatterImpl::addResult(absl::string_view name,
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

JsonOutputFormatterImpl::JsonOutputFormatterImpl(Envoy::TimeSource& time_source,
                                                 const Options& options)
    : OutputFormatterImpl(time_source, options) {}

std::string JsonOutputFormatterImpl::toString() const {
  return Envoy::MessageUtil::getJsonStringFromMessage(toProto(), true, true);
}

YamlOutputFormatterImpl::YamlOutputFormatterImpl(Envoy::TimeSource& time_source,
                                                 const Options& options)
    : OutputFormatterImpl(time_source, options) {}

std::string YamlOutputFormatterImpl::toString() const {
  return Envoy::MessageUtil::getYamlStringFromMessage(toProto(), true, true);
}

} // namespace Client
} // namespace Nighthawk