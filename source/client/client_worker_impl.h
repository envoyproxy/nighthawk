#pragma once

#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/stats/store.h"
#include "envoy/thread_local/thread_local.h"
#include "envoy/upstream/cluster_manager.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/client/client_worker.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/uri.h"

#include "common/worker_impl.h"

namespace Nighthawk {
namespace Client {

class ClientWorkerImpl : public WorkerImpl, virtual public ClientWorker {
public:
  ClientWorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                   Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                   const BenchmarkClientFactory& benchmark_client_factory,
                   const SequencerFactory& sequencer_factory, UriPtr&& uri,
                   Envoy::Stats::Store& store, const int worker_number,
                   const Envoy::MonotonicTime starting_time, bool prefetch_connections);

  StatisticPtrMap statistics() const override;
  bool success() const override { return success_; }

  const std::map<std::string, uint64_t>& thread_local_counter_values() override {
    return thread_local_counter_values_;
  }

protected:
  void work() override;

private:
  void simpleWarmup();
  Envoy::Stats::ScopePtr worker_scope_;
  Envoy::Stats::ScopePtr worker_number_scope_;
  const int worker_number_;
  const Envoy::MonotonicTime starting_time_;
  bool success_{};
  BenchmarkClientPtr benchmark_client_;
  const SequencerPtr sequencer_;
  Envoy::LocalInfo::LocalInfoPtr local_info_;
  const bool prefetch_connections_;
  std::map<std::string, uint64_t> thread_local_counter_values_;
};

using ClientWorkerImplPtr = std::unique_ptr<ClientWorkerImpl>;

} // namespace Client
} // namespace Nighthawk