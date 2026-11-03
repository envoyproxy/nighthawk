#pragma once

#include <memory>
#include <string>

#include "nighthawk/common/factories.h"

#include "envoy/stats/sink.h"

#include "api/stats_sink/envoy_stats_sink_adapter.pb.h"

namespace Nighthawk {

/**
 * A Nighthawk stats sink that forwards what it receives to a stats sink implemented as an Envoy
 * extension, so that Envoy's sinks can be used without a Nighthawk specific implementation of
 * each.
 *
 * Flushes are forwarded unchanged. Samples of Nighthawk's latency statistics are forwarded in a
 * form an Envoy sink can interpret: Nighthawk records them in nanoseconds and declares no unit,
 * whereas Envoy sinks scale histograms by their declared unit. The adapter therefore presents
 * each such statistic as a Microseconds histogram named after its worker, and converts the sample
 * to match. Samples of any other histogram are forwarded unchanged.
 */
class EnvoyStatsSinkAdapter : public Envoy::Stats::Sink {
public:
  /**
   * @param envoy_sink the Envoy stats sink to forward to.
   */
  explicit EnvoyStatsSinkAdapter(Envoy::Stats::SinkPtr&& envoy_sink);

  // Envoy::Stats::Sink
  void flush(Envoy::Stats::MetricSnapshot& snapshot) override;
  void onHistogramComplete(const Envoy::Stats::Histogram& histogram, uint64_t value) override;

private:
  Envoy::Stats::SinkPtr envoy_sink_;
};

/**
 * Factory that creates an EnvoyStatsSinkAdapter from an EnvoyStatsSinkAdapterConfig proto.
 * Registered as an Envoy plugin.
 */
class EnvoyStatsSinkAdapterFactory : public NighthawkStatsSinkFactory {
public:
  // NighthawkStatsSinkFactory
  std::unique_ptr<Envoy::Stats::Sink>
  createStatsSink(Envoy::Stats::SymbolTable& symbol_table) override;
  std::unique_ptr<Envoy::Stats::Sink>
  createStatsSink(const envoy::config::metrics::v3::StatsSink& config,
                  Envoy::Server::Configuration::ServerFactoryContext& context) override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  std::string name() const override;
};

DECLARE_FACTORY(EnvoyStatsSinkAdapterFactory);

} // namespace Nighthawk
