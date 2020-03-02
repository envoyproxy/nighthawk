#pragma once

#include <memory>

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "envoy/event/dispatcher.h"
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
                              const Envoy::MonotonicTime scheduled_starting_time) const PURE;
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

} // namespace Nighthawk
