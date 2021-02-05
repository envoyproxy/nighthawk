#pragma once

#include <cstdint>

#include "envoy/common/time.h"

#include "nighthawk/client/options.h"
#include "nighthawk/client/output_collector.h"

namespace Nighthawk {
namespace Client {

class OutputCollectorImpl : public OutputCollector {
public:
  /**
   * @param time_source Time source that will be used to generate a timestamp in the output.
   * @param options The options that led up to the output that will be computed by this instance.
   */
  OutputCollectorImpl(Envoy::TimeSource& time_source, const Options& options);

  void addResult(absl::string_view name, const std::vector<StatisticPtr>& statistics,
                 const std::map<std::string, uint64_t>& counters,
                 const std::chrono::nanoseconds execution_duration,
                 const absl::optional<Envoy::SystemTime>& first_acquisition_time) override;
  void setOutput(const nighthawk::client::Output& output) override { output_ = output; }

  nighthawk::client::Output toProto() const override;

private:
  nighthawk::client::Output output_;
};

} // namespace Client
} // namespace Nighthawk