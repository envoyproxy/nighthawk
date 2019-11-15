#include "client/client_worker_impl.h"

#include "external/envoy/source/common/stats/symbol_table_impl.h"

#include "common/header_source_impl.h"
#include "common/utility.h"

namespace Nighthawk {
namespace Client {

ClientWorkerImpl::ClientWorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                                   Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                                   const BenchmarkClientFactory& benchmark_client_factory,
                                   const TerminationPredicateFactory& termination_predicate_factory,
                                   const SequencerFactory& sequencer_factory,
                                   const HeaderSourceFactory& header_generator_factory,
                                   Envoy::Stats::Store& store, const int worker_number,
                                   const Envoy::MonotonicTime starting_time,
                                   Envoy::Tracing::HttpTracerPtr& http_tracer,
                                   bool prefetch_connections)
    : WorkerImpl(api, tls, store), worker_scope_(store_.createScope("cluster.")),
      worker_number_scope_(worker_scope_->createScope(fmt::format("{}.", worker_number))),
      worker_number_(worker_number), starting_time_(starting_time), http_tracer_(http_tracer),
      header_generator_(header_generator_factory.create()),
      benchmark_client_(benchmark_client_factory.create(
          api, *dispatcher_, *worker_number_scope_, cluster_manager, http_tracer_,
          fmt::format("{}", worker_number), *header_generator_)),
      termination_predicate_(
          termination_predicate_factory.create(time_source_, *worker_number_scope_, starting_time)),
      sequencer_(sequencer_factory.create(time_source_, *dispatcher_, starting_time,
                                          *benchmark_client_, *termination_predicate_,
                                          *worker_number_scope_)),
      prefetch_connections_(prefetch_connections) {}

void ClientWorkerImpl::simpleWarmup() {
  ENVOY_LOG(debug, "> worker {}: warmup start.", worker_number_);
  if (prefetch_connections_) {
    benchmark_client_->prefetchPoolConnections();
  }
  if (benchmark_client_->tryStartRequest([this](bool, bool) { dispatcher_->exit(); })) {
    dispatcher_->run(Envoy::Event::Dispatcher::RunType::RunUntilExit);
  } else {
    ENVOY_LOG(warn, "> worker {}: failed to initiate warmup request.", worker_number_);
  }
  ENVOY_LOG(debug, "> worker {}: warmup done.", worker_number_);
}

void ClientWorkerImpl::work() {
  simpleWarmup();
  benchmark_client_->setMeasureLatencies(true);
  sequencer_->start();
  sequencer_->waitForCompletion();
  // Save a final snapshot of the worker-specific counter accumulations before
  // we exit the thread.
  for (const auto& stat : store_.counters()) {
    // First, we strip the cluster prefix
    std::string stat_name = std::string(absl::StripPrefix(stat->name(), "cluster."));
    stat_name = std::string(absl::StripPrefix(stat_name, "worker."));
    // Second, we strip our own prefix if it's there, else we skip.
    const std::string worker_prefix = fmt::format("{}.", worker_number_);
    if (stat->value() && absl::StartsWith(stat_name, worker_prefix)) {
      thread_local_counter_values_[std::string(absl::StripPrefix(stat_name, worker_prefix))] =
          stat->value();
    }
  }
  // Note that benchmark_client_ is not terminated here, but in shutdownThread() below. This is to
  // to prevent the shutdown artifacts from influencing the test result counters. The main thread
  // still needs to be able to read the counters for reporting the global numbers, and those should
  // be consistent.
}

void ClientWorkerImpl::shutdownThread() { benchmark_client_->terminate(); }

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