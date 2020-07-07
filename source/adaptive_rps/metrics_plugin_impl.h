#pragma once

#include "absl/container/flat_hash_map.h"
#include "api/adaptive_rps/metrics_plugin_impl.pb.h"
#include "api/client/options.pb.h"
#include "api/client/output.pb.h"
#include "nighthawk/adaptive_rps/metrics_plugin.h"

namespace Nighthawk {
namespace AdaptiveRps {

// A factory that creates an ExampleMetricsPlugin initialized with a custom config proto unpacked
// from an Any proto. You must implement a similar factory for your MetricsPlugin. Part of an
// example showing how to create and register a MetricsPlugin.
class ExampleMetricsPluginConfigFactory : public MetricsPluginConfigFactory {
public:
  std::string name() const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  MetricsPluginPtr createMetricsPlugin(const Envoy::Protobuf::Message& config) override;
};

// An example MetricsPlugin that configures itself with server info from a custom config proto.
// Instead of connecting to a server, it returns a dummy value 5.0 for any metric name. Part of an
// example showing how to create and register a MetricsPlugin.
class ExampleMetricsPlugin : public MetricsPlugin {
public:
  explicit ExampleMetricsPlugin(const nighthawk::adaptive_rps::ExampleMetricsPluginConfig& config);
  double GetMetricByName(const std::string& metric_name) override;
  std::vector<std::string> GetAllSupportedMetricNames() override;

private:
  std::string address_;
  std::string credentials_;
};

// Emulated MetricPlugin that translates Nighthawk Service counters and stats into the MetricPlugin
// interface, rather than connecting to an outside source for the metrics. This class does not
// register itself with the Envoy registry mechanism. It will be constructed on the fly from each
// Nighthawk Service result.
class NighthawkStatsEmulatedMetricsPlugin : public MetricsPlugin {
public:
  explicit NighthawkStatsEmulatedMetricsPlugin(const nighthawk::client::Output& nighthawk_output);
  double GetMetricByName(const std::string& metric_name) override;
  std::vector<std::string> GetAllSupportedMetricNames() override;

private:
  absl::flat_hash_map<std::string, double> metric_from_name_;
};

} // namespace AdaptiveRps
} // namespace Nighthawk
