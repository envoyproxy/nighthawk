#include "client/output_collector_impl.h"

#include <google/protobuf/util/time_util.h>

#include <chrono>
#include <sstream>

#include "external/envoy/source/common/protobuf/utility.h"

#include "common/version_info.h"

namespace Nighthawk {
namespace Client {

OutputCollectorImpl::OutputCollectorImpl(Envoy::TimeSource& time_source, const Options& options) {
  *(output_.mutable_timestamp()) = Envoy::Protobuf::util::TimeUtil::NanosecondsToTimestamp(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          time_source.systemTime().time_since_epoch())
          .count());
  output_.set_allocated_options(options.toCommandLineOptions().release());
  *output_.mutable_version() = VersionInfo::buildVersion();
}

nighthawk::client::Output OutputCollectorImpl::toProto() const { return output_; }

void OutputCollectorImpl::addResult(
    absl::string_view name, const std::vector<StatisticPtr>& statistics,
    const std::map<std::string, uint64_t>& counters,
    const std::chrono::nanoseconds execution_duration,
    const absl::optional<Envoy::SystemTime>& first_acquisition_time) {
  auto result = output_.add_results();
  result->set_name(name.data(), name.size());
  if (first_acquisition_time.has_value()) {
    *(result->mutable_execution_start()) = Envoy::Protobuf::util::TimeUtil::NanosecondsToTimestamp(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            first_acquisition_time.value().time_since_epoch())
            .count());
  }
  for (auto& statistic : statistics) {
    // TODO(#292): Looking at if the statistic id ends with "_size" to determine how it should be
    // serialized is kind of hacky. Maybe we should have a lookup table of sorts, to determine how
    // statistics should we serialized. Doing so may give us a canonical place to consolidate their
    // ids as well too.
    Statistic::SerializationDomain serialization_domain =
        absl::EndsWith(statistic->id(), "_size") ? Statistic::SerializationDomain::RAW
                                                 : Statistic::SerializationDomain::DURATION;
    *(result->add_statistics()) = statistic->toProto(serialization_domain);
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