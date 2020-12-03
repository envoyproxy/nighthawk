#pragma once

#include <memory>

#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "nighthawk/common/statistic.h"

#include "absl/types/optional.h"

namespace Nighthawk {
namespace Client {

/**
 * Facilitates building up an output proto from Nighthawk's native data structures.
 */
class OutputCollector {
public:
  virtual ~OutputCollector() = default;

  /**
   * Adds a result to the structured output.
   *
   * @param name unique name of the result. E.g. worker_1.
   * @param statistics Reference to a vector of statistics to add to the output.
   * @param counters Reference to a map of counter values, keyed by name, to add to the output.
   * @param execution_duration Execution duration associated to the to-be-added result.
   * @param first_acquisition_time Timing of the first rate limiter acquisition.
   */
  virtual void addResult(absl::string_view name, const std::vector<StatisticPtr>& statistics,
                         const std::map<std::string, uint64_t>& counters,
                         const std::chrono::nanoseconds execution_duration,
                         const absl::optional<Envoy::SystemTime>& first_acquisition_time) PURE;
  /**
   * Directly sets the output value.
   *
   * @param output the output value to set.
   */
  virtual void setOutput(const nighthawk::client::Output& output) PURE;

  /**
   * @return nighthawk::client::Output proto output object.
   */
  virtual nighthawk::client::Output toProto() const PURE;
};

using OutputCollectorPtr = std::unique_ptr<OutputCollector>;

} // namespace Client
} // namespace Nighthawk
