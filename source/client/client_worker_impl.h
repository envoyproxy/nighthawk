#pragma once

#include "envoy/api/api.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/client/client_worker.h"
#include "nighthawk/client/factories.h"
#include "nighthawk/common/sequencer.h"

#include "nighthawk/source/common/utility.h"
#include "nighthawk/source/common/worker_impl.h"

namespace Nighthawk {
namespace Client {

class ClientWorkerImpl : public WorkerImpl,
                         virtual public ClientWorker,
                         Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  ClientWorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                   const BenchmarkClientFactory& benchmark_client_factory,
                   const SequencerFactory& sequencer_factory, const Uri uri,
                   Envoy::Stats::StorePtr&& store, const int worker_number,
                   const Envoy::MonotonicTime starting_time);

  StatisticPtrMap statistics() const override;
  Envoy::Stats::Store& store() const override { return *store_; }
  bool success() const override { return success_; }

protected:
  void work() override;

private:
  void simpleWarmup();
  const Uri uri_;
  const int worker_number_;
  const Envoy::MonotonicTime starting_time_;
  bool success_{};
  const BenchmarkClientPtr benchmark_client_;
  const SequencerPtr sequencer_;
};

typedef std::unique_ptr<ClientWorkerImpl> ClientWorkerImplPtr;

} // namespace Client
} // namespace Nighthawk