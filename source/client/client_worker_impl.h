#pragma once

#include <vector>

#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/stats/store.h"
#include "envoy/thread_local/thread_local.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/client/client_worker.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/common/factories.h"
#include "nighthawk/common/phase.h"
#include "nighthawk/common/request_source.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/termination_predicate.h"
#include "nighthawk/user_defined_output/user_defined_output_plugin.h"

#include "source/common/worker_impl.h"

namespace Nighthawk {
namespace Client {

class ClientWorkerImpl : public WorkerImpl, virtual public ClientWorker {
public:
  enum class HardCodedWarmupStyle { OFF, ON };

  ClientWorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                   Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                   const BenchmarkClientFactory& benchmark_client_factory,
                   const TerminationPredicateFactory& termination_predicate_factory,
                   const SequencerFactory& sequencer_factory,
                   const RequestSourceFactory& request_generator_factory,
                   Envoy::Stats::Store& store, const int worker_number,
                   const Envoy::MonotonicTime starting_time,
                   Envoy::Tracing::HttpTracerSharedPtr& http_tracer,
                   const HardCodedWarmupStyle hardcoded_warmup_style,
                   std::vector<UserDefinedOutputPluginPtr> user_defined_output_plugins);
  StatisticPtrMap statistics() const override;

  const std::map<std::string, uint64_t>& threadLocalCounterValues() override {
    return threadLocalCounterValues_;
  }

  const Phase& phase() const override { return *phase_; }

  void shutdownThread() override;

  void requestExecutionCancellation() override;

  /**
   * Returns additional output from any specified User Defined Output plugins.
   *
   * @return vector of Envoy::ProtobufWkt::Any, each of which may be a different underlying proto.
   */
  std::vector<Envoy::ProtobufWkt::Any> getUserDefinedOutputResults() const override;

protected:
  void work() override;

private:
  void simpleWarmup();

  std::unique_ptr<Envoy::TimeSource> time_source_;
  const TerminationPredicateFactory& termination_predicate_factory_;
  const SequencerFactory& sequencer_factory_;
  Envoy::Stats::ScopeSharedPtr worker_scope_;
  Envoy::Stats::ScopeSharedPtr worker_number_scope_;
  const int worker_number_;
  Envoy::Tracing::HttpTracerSharedPtr& http_tracer_;
  RequestSourcePtr request_generator_;
  BenchmarkClientPtr benchmark_client_;
  PhasePtr phase_;
  Envoy::LocalInfo::LocalInfoPtr local_info_;
  std::map<std::string, uint64_t> threadLocalCounterValues_;
  const HardCodedWarmupStyle hardcoded_warmup_style_;
};

using ClientWorkerImplPtr = std::unique_ptr<ClientWorkerImpl>;

} // namespace Client
} // namespace Nighthawk
