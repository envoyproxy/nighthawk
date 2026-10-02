#include "source/stats_sink/envoy_stats_sink_adapter.h"

#include <optional>

#include "envoy/registry/registry.h"

#include "source/common/config/utility.h"
#include "source/common/config/well_known_names.h"
#include "source/common/protobuf/utility.h"
#include "source/server/configuration_impl.h"

#include "source/common/statistic_impl.h"

#include "fmt/format.h"

namespace Nighthawk {
namespace {

// A read-only view of one of Nighthawk's latency statistics, as it is presented to an Envoy
// stats sink. Sinks receive a const reference, so only the const interface is reachable.
class LatencyHistogramView : public Envoy::Stats::Histogram {
public:
  LatencyHistogramView(const Envoy::Stats::Histogram& histogram, std::optional<int> worker_id)
      : histogram_(histogram), worker_id_(worker_id) {}

  // Envoy::Stats::Histogram
  Unit unit() const override { return Unit::Microseconds; }
  void recordValue(uint64_t) override { PANIC("LatencyHistogramView is read-only"); }

  // Envoy::Stats::Metric
  // Named like the other statistics of a Nighthawk worker, which live in the "cluster.<n>." scope
  // and have the worker extracted as the cluster name tag.
  std::string name() const override {
    return worker_id_.has_value() ? fmt::format("cluster.{}.{}", *worker_id_, histogram_.name())
                                  : histogram_.name();
  }
  std::string tagExtractedName() const override {
    return worker_id_.has_value() ? fmt::format("cluster.{}", histogram_.name())
                                  : histogram_.name();
  }
  Envoy::Stats::TagVector tags() const override {
    Envoy::Stats::TagVector tags = histogram_.tags();
    if (worker_id_.has_value()) {
      tags.push_back({Envoy::Config::TagNames::get().CLUSTER_NAME, fmt::format("{}", *worker_id_)});
    }
    return tags;
  }
  Envoy::Stats::StatName statName() const override { return histogram_.statName(); }
  Envoy::Stats::StatName tagExtractedStatName() const override {
    return histogram_.tagExtractedStatName();
  }
  void iterateTagStatNames(const TagStatNameIterFn& fn) const override {
    histogram_.iterateTagStatNames(fn);
  }
  bool used() const override { return histogram_.used(); }
  void markUnused() override { PANIC("LatencyHistogramView is read-only"); }
  bool hidden() const override { return histogram_.hidden(); }
  bool noTagExtraction() const override { return histogram_.noTagExtraction(); }
  void markAsNoTagExtraction() override { PANIC("LatencyHistogramView is read-only"); }
  Envoy::Stats::SymbolTable& symbolTable() override { PANIC("LatencyHistogramView is read-only"); }
  const Envoy::Stats::SymbolTable& constSymbolTable() const override {
    return histogram_.constSymbolTable();
  }

  // Envoy::Stats::RefcountInterface
  // The view lives on the stack for the duration of one onHistogramComplete() call and is never
  // reference counted.
  void incRefCount() override { PANIC("LatencyHistogramView is not reference counted"); }
  bool decRefCount() override { PANIC("LatencyHistogramView is not reference counted"); }
  uint32_t use_count() const override { return 1; }

private:
  const Envoy::Stats::Histogram& histogram_;
  const std::optional<int> worker_id_;
};

// Nighthawk records latencies in nanoseconds. Rounded rather than truncated, which would bias
// every sample low by up to a microsecond.
uint64_t nanosecondsToMicroseconds(uint64_t nanoseconds) { return (nanoseconds + 500) / 1000; }

} // namespace

EnvoyStatsSinkAdapter::EnvoyStatsSinkAdapter(Envoy::Stats::SinkPtr&& envoy_sink)
    : envoy_sink_(std::move(envoy_sink)) {}

void EnvoyStatsSinkAdapter::flush(Envoy::Stats::MetricSnapshot& snapshot) {
  envoy_sink_->flush(snapshot);
}

void EnvoyStatsSinkAdapter::onHistogramComplete(const Envoy::Stats::Histogram& histogram,
                                                uint64_t value) {
  const SinkableStatistic* statistic = dynamic_cast<const SinkableStatistic*>(&histogram);
  if (statistic == nullptr) {
    // One of Envoy's own histograms, which already declares its unit.
    envoy_sink_->onHistogramComplete(histogram, value);
    return;
  }
  const LatencyHistogramView view(histogram, statistic->worker_id());
  envoy_sink_->onHistogramComplete(view, nanosecondsToMicroseconds(value));
}

std::unique_ptr<Envoy::Stats::Sink>
EnvoyStatsSinkAdapterFactory::createStatsSink(Envoy::Stats::SymbolTable&) {
  throw Envoy::EnvoyException(fmt::format(
      "{} must be created with its configuration and a server factory context", name()));
}

std::unique_ptr<Envoy::Stats::Sink> EnvoyStatsSinkAdapterFactory::createStatsSink(
    const envoy::config::metrics::v3::StatsSink& config,
    Envoy::Server::Configuration::ServerFactoryContext& context) {
  nighthawk::EnvoyStatsSinkAdapterConfig adapter_config;
  THROW_IF_NOT_OK(Envoy::MessageUtil::unpackTo(config.typed_config(), adapter_config));

  Envoy::Server::Configuration::StatsSinkFactory& envoy_factory =
      Envoy::Config::Utility::getAndCheckFactory<Envoy::Server::Configuration::StatsSinkFactory>(
          adapter_config.sink());
  Envoy::ProtobufTypes::MessagePtr envoy_config = Envoy::Config::Utility::translateToFactoryConfig(
      adapter_config.sink(), context.messageValidationContext().staticValidationVisitor(),
      envoy_factory);
  absl::StatusOr<Envoy::Stats::SinkPtr> envoy_sink =
      envoy_factory.createStatsSink(*envoy_config, context);
  THROW_IF_NOT_OK_REF(envoy_sink.status());
  return std::make_unique<EnvoyStatsSinkAdapter>(std::move(envoy_sink.value()));
}

Envoy::ProtobufTypes::MessagePtr EnvoyStatsSinkAdapterFactory::createEmptyConfigProto() {
  return std::make_unique<nighthawk::EnvoyStatsSinkAdapterConfig>();
}

std::string EnvoyStatsSinkAdapterFactory::name() const {
  return "nighthawk.envoy_stats_sink_adapter";
}

REGISTER_FACTORY(EnvoyStatsSinkAdapterFactory, NighthawkStatsSinkFactory);

} // namespace Nighthawk
