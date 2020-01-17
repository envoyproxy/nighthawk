#pragma once

#include <memory>

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "envoy/event/dispatcher.h"
#include "envoy/stats/store.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/client/options.h"
#include "nighthawk/client/output_formatter.h"
#include "nighthawk/common/platform_util.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/termination_predicate.h"
#include "nighthawk/common/uri.h"

namespace Nighthawk {
namespace Client {

class BenchmarkClientFactory {
public:
  virtual ~BenchmarkClientFactory() = default;
  virtual BenchmarkClientPtr create(Envoy::Api::Api& api, Envoy::Event::Dispatcher& dispatcher,
                                    Envoy::Stats::Scope& scope,
                                    Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                                    Envoy::Tracing::HttpTracerPtr& http_tracer,
                                    absl::string_view cluster_name,
                                    RequestSource& request_generator) const PURE;
};

class SequencerFactory {
public:
  virtual ~SequencerFactory() = default;
  virtual SequencerPtr create(Envoy::TimeSource& time_source, Envoy::Event::Dispatcher& dispatcher,
                              BenchmarkClient& benchmark_client,
                              TerminationPredicatePtr&& termination_predicate,
                              Envoy::Stats::Scope& scope,
                              const Envoy::MonotonicTime scheduled_starting_time) const PURE;
};

class StoreFactory {
public:
  virtual ~StoreFactory() = default;
  virtual Envoy::Stats::StorePtr create() const PURE;
};

class StatisticFactory {
public:
  virtual ~StatisticFactory() = default;
  virtual StatisticPtr create() const PURE;
};

class OutputFormatterFactory {
public:
  virtual ~OutputFormatterFactory() = default;
  virtual OutputFormatterPtr
  create(const nighthawk::client::OutputFormat_OutputFormatOptions) const PURE;
};

} // namespace Client

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
