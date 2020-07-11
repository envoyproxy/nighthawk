#pragma once

#include "nighthawk/adaptive_load/metrics_plugin.h"

#include "api/adaptive_load/metrics_plugin_impl.pb.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"

#include "absl/container/flat_hash_map.h"

namespace Nighthawk {
namespace AdaptiveLoad {

// An example MetricsPlugin configured with info from a custom config proto. Instead of contacting a
// server, it returns dummy values. Part of an example showing how to create and register a
// MetricsPlugin.
class ExampleMetricsPlugin : public MetricsPlugin {
public:
  explicit ExampleMetricsPlugin(const nighthawk::adaptive_load::ExampleMetricsPluginConfig& config);
  double GetMetricByName(const std::string& metric_name) override;
  std::vector<std::string> GetAllSupportedMetricNames() override;

private:
  std::string address_;
  std::string credentials_;
};

// A factory that creates an ExampleMetricsPlugin from a custom config proto unpacked from an Any
// proto. You must implement a similar factory for your MetricsPlugin. Part of an example showing
// how to create and register a MetricsPlugin.
class ExampleMetricsPluginConfigFactory : public MetricsPluginConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  MetricsPluginPtr createMetricsPlugin(const Envoy::Protobuf::Message& config) override;
};

// Emulated MetricPlugin that wraps already collected Nighthawk Service counters and stats in a
// MetricPlugin interface. This class is not registered with the Envoy registry mechanism. It will
// be constructed on the fly from each Nighthawk Service result.
class NighthawkStatsEmulatedMetricsPlugin : public MetricsPlugin {
public:
  explicit NighthawkStatsEmulatedMetricsPlugin(const nighthawk::client::Output& nighthawk_output);
  double GetMetricByName(const std::string& metric_name) override;
  std::vector<std::string> GetAllSupportedMetricNames() override;

private:
  absl::flat_hash_map<std::string, double> metric_from_name_;
};

} // namespace AdaptiveLoad
} // namespace Nighthawk
