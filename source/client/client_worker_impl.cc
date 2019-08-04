#include "client/client_worker_impl.h"

#include "common/http/header_map_impl.h"
#include "common/http/headers.h"
#include "common/http/http1/conn_pool.h"
#include "common/http/http2/conn_pool.h"
#include "common/http/utility.h"
#include "common/network/dns_impl.h"
#include "common/network/raw_buffer_socket.h"
#include "common/network/utility.h"
#include "common/protobuf/message_validator_impl.h"
#include "common/upstream/cluster_manager_impl.h"
#include "common/upstream/upstream_impl.h"

namespace Nighthawk {
namespace Client {

ClientWorkerImpl::ClientWorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                                   Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                                   const BenchmarkClientFactory& benchmark_client_factory,
                                   const SequencerFactory& sequencer_factory, UriPtr&& uri,
                                   Envoy::Stats::StorePtr&& store, const int worker_number,
                                   const Envoy::MonotonicTime starting_time)
    : WorkerImpl(api, tls, std::move(store)), worker_number_(worker_number),
      starting_time_(starting_time),
      benchmark_client_(benchmark_client_factory.create(api, *dispatcher_, *store_, std::move(uri),
                                                        cluster_manager)),
      sequencer_(
          sequencer_factory.create(time_source_, *dispatcher_, starting_time, *benchmark_client_)),
      cluster_manager_(cluster_manager) {

  //  benchmark_client_ = benchmark_client_factory.create(api, *dispatcher_, *store_,
  //  std::move(uri),
  //                                                      *cluster_manager_);

  std::cerr << "client worker get : " << &cluster_manager_ << std::endl;

  // auto foo = cluster_manager_.get("staticcluster");
  // std::cerr << "client worker get staticcluster: " << &foo << std::endl;
  // foo->loadBalancer().chooseHost(nullptr);
  // foo->info()->
}

void ClientWorkerImpl::simpleWarmup() {
  ENVOY_LOG(debug, "> worker {}: warmup start.", worker_number_);
  benchmark_client_->tryStartOne([this] { dispatcher_->exit(); });
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
  ENVOY_LOG(debug, "> worker {}: warmup done.", worker_number_);
}

void ClientWorkerImpl::work() {
  std::cerr << "ClientWorkerImpl::work " << dispatcher_.get() << std::endl;
  benchmark_client_->initialize(*Envoy::Runtime::LoaderSingleton::getExisting(), tls_);
  simpleWarmup();
  benchmark_client_->setMeasureLatencies(true);
  sequencer_->start();
  sequencer_->waitForCompletion();
  benchmark_client_->terminate();
  success_ = true;
  dispatcher_->exit();
}

StatisticPtrMap ClientWorkerImpl::statistics() const {
  StatisticPtrMap statistics;
  StatisticPtrMap s1 = benchmark_client_->statistics();
  StatisticPtrMap s2 = sequencer_->statistics();
  statistics.insert(s1.begin(), s1.end());
  statistics.insert(s2.begin(), s2.end());
  return statistics;
}

} // namespace Client
} // namespace Nighthawk