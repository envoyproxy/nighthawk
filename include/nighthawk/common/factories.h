#pragma once

#include <memory>
#include <vector>

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "envoy/event/dispatcher.h"
#include "envoy/thread_local/thread_local.h"
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
   * @param config the sink's typed_config, already translated to the message type returned by
   * createEmptyConfigProto() and validated.
   * @param symbol_table supplies the symbol_table instance. For the definition
   * of SymbolTable, see envoy/include/envoy/stats/symbol_table.h.
   * @param tls thread local slot allocator, for sinks that keep per-thread state such as a
   * socket. Sinks are created on the main thread before the flush worker starts, and may be
   * invoked from the flush worker thread (flush) and from the client worker threads
   * (onHistogramComplete).
   * @param tags "key:value" tags the user asked to attach to every metric; sinks that cannot
   * carry tags ignore them.
   */
  virtual std::unique_ptr<Envoy::Stats::Sink>
  createStatsSink(const Envoy::Protobuf::Message& config, Envoy::Stats::SymbolTable& symbol_table,
                  Envoy::ThreadLocal::SlotAllocator& tls,
                  const std::vector<std::string>& tags) PURE;

  std::string category() const override { return "nighthawk.stats_sinks"; }
};

} // namespace Nighthawk
