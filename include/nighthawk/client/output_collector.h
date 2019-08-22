#pragma once

#include <memory>

#include "envoy/common/pure.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/client/options.h"
#include "nighthawk/common/statistic.h"

namespace Nighthawk {
namespace Client {

class OutputCollector {
public:
  virtual ~OutputCollector() = default;
  virtual void addResult(absl::string_view name, const std::vector<StatisticPtr>& statistics,
                         const std::map<std::string, uint64_t>& counters) PURE;
  virtual nighthawk::client::Output toProto() const PURE;
  virtual std::string toString() const PURE;
};

using OutputCollectorPtr = std::unique_ptr<OutputCollector>;

} // namespace Client
} // namespace Nighthawk
