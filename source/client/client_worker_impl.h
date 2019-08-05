#pragma once

#include "envoy/api/api.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/client/client_worker.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/common/sequencer.h"
#include "nighthawk/common/uri.h"

#include "common/utility.h"
#include "common/worker_impl.h"

#include "common/access_log/access_log_manager_impl.h"
#include "common/http/context_impl.h"

#include "common/common/logger.h"
#include "common/http/header_map_impl.h"
#include "common/runtime/runtime_impl.h"
#include "common/ssl.h"

#include "common/upstream/cluster_manager_impl.h"

namespace Nighthawk {
namespace Client {

class ClientWorkerImpl : public WorkerImpl,
                         virtual public ClientWorker,
                         Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  ClientWorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                   Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                   const BenchmarkClientFactory& benchmark_client_factory,
                   const SequencerFactory& sequencer_factory, UriPtr&& uri,
                   Envoy::Stats::Store& store, const int worker_number,
                   const Envoy::MonotonicTime starting_time);

  StatisticPtrMap statistics() const override;
  Envoy::Stats::Store& store() const override { return store_; }
  bool success() const override { return success_; }

protected:
  void work() override;

private:
  void simpleWarmup();
  const int worker_number_;
  const Envoy::MonotonicTime starting_time_;
  bool success_{};
  BenchmarkClientPtr benchmark_client_;
  const SequencerPtr sequencer_;

  Ssl::FakeAdmin admin_;
  Envoy::LocalInfo::LocalInfoPtr local_info_;
};

using ClientWorkerImplPtr = std::unique_ptr<ClientWorkerImpl>;

} // namespace Client
} // namespace Nighthawk