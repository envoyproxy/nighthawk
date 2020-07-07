#include "adaptive_rps/metrics_plugin_impl.h"

#include "envoy/registry/registry.h"

#include "external/envoy/source/common/protobuf/protobuf.h"

namespace Nighthawk {
namespace AdaptiveRps {

// The name of the plugin, to be referenced from AdaptiveRpsSessionSpec.
std::string ExampleMetricsPluginConfigFactory::name() const { return "example-metrics-plugin"; }

// A method required by the Envoy plugin system. The proto created here is only ever used to display
// its type name. The config proto actually passed to the plugin's constructor is created on the
// stack in ExampleMetricsPluginConfigFactory::createMetricsPlugin().
Envoy::ProtobufTypes::MessagePtr ExampleMetricsPluginConfigFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::adaptive_rps::ExampleMetricsPluginConfig>();
}

// Unpacks the Any config proto to the plugin-specific ExampleMetricsPluginConfig, then instantiates
// ExampleMetricsPlugin with the strongly typed config object.
MetricsPluginPtr
ExampleMetricsPluginConfigFactory::createMetricsPlugin(const Envoy::Protobuf::Message& message) {
  const Envoy::ProtobufWkt::Any& any = dynamic_cast<const Envoy::ProtobufWkt::Any&>(message);
  nighthawk::adaptive_rps::ExampleMetricsPluginConfig config;
  Envoy::MessageUtil::unpackTo(any, config);
  return std::make_unique<ExampleMetricsPlugin>(config);
}

// Registers the factory for ExampleMetricsPlugin in the Envoy registry.
//
// !!! Don't forget REGISTER_FACTORY !!!
//
REGISTER_FACTORY(ExampleMetricsPluginConfigFactory, MetricsPluginConfigFactory);

ExampleMetricsPlugin::ExampleMetricsPlugin(
    const nighthawk::adaptive_rps::ExampleMetricsPluginConfig& config)
    : address_{config.address()}, credentials_{config.credentials()} {}

double ExampleMetricsPlugin::GetMetricByName(const std::string& metric_name) {
  // Real plugin would query an outside server or other data source.
  if (metric_name == "example_metric1") {
    return 5.0;
  } else {
    return 15.0;
  }
}

std::vector<std::string> ExampleMetricsPlugin::GetAllSupportedMetricNames() {
  return {"example_metric1", "example_metric2"};
}

NighthawkStatsEmulatedMetricsPlugin::NighthawkStatsEmulatedMetricsPlugin(
    const nighthawk::client::Output& nighthawk_output) {
  for (const nighthawk::client::Result& result : nighthawk_output.results()) {
    if (result.name() != "global") {
      continue;
    }
    const int total_specified = nighthawk_output.options().requests_per_second().value() *
                                nighthawk_output.options().duration().seconds();
    int total_sent = 0;
    int total_2xx = 0;
    for (const nighthawk::client::Counter& counter : result.counters()) {
      if (counter.name() == "benchmark.http_2xx") {
        total_2xx = counter.value();
      } else if (counter.name() == "upstream_rq_total") {
        total_sent = counter.value();
      }
    }
    metric_from_name_["attempted-rps"] =
        total_specified / nighthawk_output.options().duration().seconds();
    metric_from_name_["achieved-rps"] =
        static_cast<double>(total_sent) / nighthawk_output.options().duration().seconds();
    metric_from_name_["send-rate"] = static_cast<double>(total_sent) / total_specified;
    metric_from_name_["success-rate"] = static_cast<double>(total_2xx) / total_sent;

    for (const nighthawk::client::Statistic& statistic : result.statistics()) {
      if (statistic.id() == "benchmark_http_client.request_to_response") {
        double min = statistic.min().seconds() * 1.0e9 + statistic.min().nanos();
        double mean = statistic.mean().seconds() * 1.0e9 + statistic.mean().nanos();
        double max = statistic.max().seconds() * 1.0e9 + statistic.max().nanos();
        double stdev = statistic.pstdev().seconds() * 1.0e9 + statistic.pstdev().nanos();
        metric_from_name_["latency-ns-min"] = min;
        metric_from_name_["latency-ns-mean"] = mean;
        metric_from_name_["latency-ns-max"] = max;
        metric_from_name_["latency-ns-mean-plus-1stdev"] = mean + stdev;
        metric_from_name_["latency-ns-mean-plus-2stdev"] = mean + 2 * stdev;
        metric_from_name_["latency-ns-mean-plus-3stdev"] = mean + 3 * stdev;
        break;
      }
    }
  }
}

double NighthawkStatsEmulatedMetricsPlugin::GetMetricByName(const std::string& metric_name) {
  return metric_from_name_[metric_name];
}

std::vector<std::string> NighthawkStatsEmulatedMetricsPlugin::GetAllSupportedMetricNames() {
  return {
      "attempted-rps",
      "achieved-rps",
      "send-rate",
      "success-rate",
      "latency-ns-min",
      "latency-ns-mean",
      "latency-ns-max",
      "latency-ns-mean-plus-1stdev",
      "latency-ns-mean-plus-2stdev",
      "latency-ns-mean-plus-3stdev",
  };
}

// Note: Don't use REGISTER_FACTORY for NighthawkStatsEmulatedMetricsPlugin. See header for details.

} // namespace AdaptiveRps
} // namespace Nighthawk
