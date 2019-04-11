#include "client/client_worker_impl.h"

namespace Nighthawk {
namespace Client {

ClientWorkerImpl::ClientWorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                                   const BenchmarkClientFactory& benchmark_client_factory,
                                   const SequencerFactory& sequencer_factory, const Uri uri,
                                   Envoy::Stats::StorePtr&& store, const int worker_number,
                                   const Envoy::MonotonicTime starting_time)
    : WorkerImpl(api, tls, std::move(store)), uri_(uri), worker_number_(worker_number),
      starting_time_(starting_time),
      benchmark_client_(benchmark_client_factory.create(api, *dispatcher_, *store_, uri)),
      sequencer_(sequencer_factory.create(time_source_, *dispatcher_, starting_time,
                                          *benchmark_client_)) {}

void ClientWorkerImpl::simpleWarmup() {
  ENVOY_LOG(debug, "> worker {}: warming up.", worker_number_);
  // TODO(oschaaf): Maybe add BenchmarkClient::warmup() and call that here.
  // Ideally we prefetch the requested amount of connections.
  // Currently it is possible to use less connections then specified if
  // completions are fast enough. While this may be an asset, it may also be annoying
  // when comparing results to some other tools, which do open up the specified amount
  // of connections.
  benchmark_client_->tryStartOne([this] { dispatcher_->exit(); });
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
}

void ClientWorkerImpl::work() {
  benchmark_client_->initialize(*Envoy::Runtime::LoaderSingleton::getExisting());
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