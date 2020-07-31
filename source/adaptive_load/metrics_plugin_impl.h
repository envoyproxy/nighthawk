#pragma once

#include "nighthawk/adaptive_load/metrics_plugin.h"

#include "api/adaptive_load/metrics_plugin_impl.pb.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"

#include "absl/container/flat_hash_map.h"

namespace Nighthawk {

// Emulated MetricPlugin that wraps already collected Nighthawk Service counters and stats in a
// MetricPlugin interface. This class is not registered with the Envoy registry mechanism. It will
// be constructed on the fly from each Nighthawk Service result.
class NighthawkStatsEmulatedMetricsPlugin : public MetricsPlugin {
public:
  explicit NighthawkStatsEmulatedMetricsPlugin(const nighthawk::client::Output& nighthawk_output);
  double GetMetricByName(const std::string& metric_name) override;
  const std::vector<std::string> GetAllSupportedMetricNames() const override;

private:
  absl::flat_hash_map<std::string, double> metric_from_name_;
};

} // namespace Nighthawk
