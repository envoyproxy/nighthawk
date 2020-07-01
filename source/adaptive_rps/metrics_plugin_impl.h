#pragma once

#include "absl/container/flat_hash_map.h"
#include "nighthawk/adaptive_rps/metrics_plugin.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"

namespace Nighthawk {
namespace AdaptiveRps {

// Special MetricPlugin that translates Nighthawk Service counters and stats
// into the MetricPlugin interface, rather than connecting to an outside source
// for the metrics. This plugin does not register itself with the Envoy registry
// mechanism because it needs to be constructed specially by the adaptive RPS
// controller rather than with the generic config proto.
class InternalMetricsPlugin : public MetricsPlugin {
public:
  explicit InternalMetricsPlugin(const nighthawk::client::Output& nighthawk_output);
  double GetMetricByName(const std::string& metric_name) override;

private:
  absl::flat_hash_map<std::string, double> metric_from_name_;
};

} // namespace AdaptiveRps
} // namespace Nighthawk
