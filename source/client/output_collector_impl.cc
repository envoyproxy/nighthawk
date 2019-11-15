#include "client/output_collector_impl.h"

#include <google/protobuf/util/time_util.h>

#include <chrono>
#include <sstream>

#include "external/envoy/source/common/protobuf/utility.h"

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

void OutputCollectorImpl::addResult(absl::string_view name,
                                    const std::vector<StatisticPtr>& statistics,
                                    const std::map<std::string, uint64_t>& counters,
                                    const std::chrono::nanoseconds execution_duration) {
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
  *result->mutable_execution_duration() =
      Envoy::Protobuf::util::TimeUtil::NanosecondsToDuration(execution_duration.count());
}

} // namespace Client
} // namespace Nighthawk