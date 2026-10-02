#pragma once

#include <memory>

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "envoy/config/metrics/v3/stats.pb.h"
#include "envoy/event/dispatcher.h"
#include "envoy/server/factory_context.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/common/platform_util.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/termination_predicate.h"

namespace Nighthawk {

class SequencerFactory {
public:
  virtual ~SequencerFactory() = default;
  virtual SequencerPtr create(Envoy::TimeSource& time_source, Envoy::Event::Dispatcher& dispatcher,
                              const SequencerTarget& sequencer_target,
                              TerminationPredicatePtr&& termination_predicate,
                              Envoy::Stats::Scope& scope,
                              const Envoy::MonotonicTime scheduled_starting_time,
                              Envoy::Api::Api& api) const PURE;
};

class StatisticFactory {
public:
  virtual ~StatisticFactory() = default;
  virtual StatisticPtr create() const PURE;
};

class RequestSourceFactory {
public:
  virtual ~RequestSourceFactory() = default;
  virtual RequestSourcePtr create(const Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                                  Envoy::Event::Dispatcher& dispatcher, Envoy::Stats::Scope& scope,
                                  absl::string_view service_cluster_name) const PURE;
};

class TerminationPredicateFactory {
public:
  virtual ~TerminationPredicateFactory() = default;
  virtual TerminationPredicatePtr
  create(Envoy::TimeSource& time_source, Envoy::Stats::Scope& scope,
         const Envoy::MonotonicTime scheduled_starting_time) const PURE;
};

/**
 * Factory Interface to create Envoy::Stats::Sink in Nighthawk.
 * Implemented for each Envoy::Stats::Sink and registered via
 * Registry::registerFactory() or the convenience class RegisterFactory.
 */
class NighthawkStatsSinkFactory : public Envoy::Config::TypedFactory {
public:
  ~NighthawkStatsSinkFactory() override = default;

  /**
   * Create a particular Envoy::Stats::Sink implementation. If the
   * implementation is unable to produce a Sink with the provided parameters, it
   * should throw an EnvoyException. The returned pointer should always be
   * valid.
   * @param symbol_table supplies the symbol_table instance. For the definition
   * of SymbolTable, see envoy/include/envoy/stats/symbol_table.h.
   */
  virtual std::unique_ptr<Envoy::Stats::Sink>
  createStatsSink(Envoy::Stats::SymbolTable& symbol_table) PURE;

  /**
   * Create a particular Envoy::Stats::Sink implementation from its configuration, with access to
   * the server factory context. Nighthawk calls this overload. The default implementation ignores
   * the configuration and delegates to createStatsSink(symbol_table), so factories that only
   * implement that method behave exactly as before.
   * @param config the stats sink configuration this factory was selected for.
   * @param context the server factory context of the Nighthawk process.
   */
  virtual std::unique_ptr<Envoy::Stats::Sink>
  createStatsSink(const envoy::config::metrics::v3::StatsSink& config,
                  Envoy::Server::Configuration::ServerFactoryContext& context) {
    UNREFERENCED_PARAMETER(config);
    return createStatsSink(context.scope().symbolTable());
  }

  std::string category() const override { return "nighthawk.stats_sinks"; }
};

} // namespace Nighthawk
