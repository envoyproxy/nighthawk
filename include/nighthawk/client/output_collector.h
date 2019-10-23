#pragma once

#include <memory>

#include "envoy/common/pure.h"

#include "nighthawk/common/statistic.h"

namespace Nighthawk {
namespace Client {

/**
 * Facilitates building up an output proto from Nighthawk's native data structures.
 */
class OutputCollector {
public:
  virtual ~OutputCollector() = default;
  virtual void addResult(absl::string_view name, const std::vector<StatisticPtr>& statistics,
                         const std::map<std::string, uint64_t>& counters,
                         const std::chrono::nanoseconds execution_duration) PURE;
  virtual nighthawk::client::Output toProto() const PURE;
};

using OutputCollectorPtr = std::unique_ptr<OutputCollector>;

} // namespace Client
} // namespace Nighthawk
