#include "client/client_worker_impl.h"

#include <vector>

#include "external/envoy/source/common/stats/symbol_table_impl.h"

#include "common/phase_impl.h"
#include "common/termination_predicate_impl.h"
#include "common/utility.h"

namespace Nighthawk {
namespace Client {

using namespace std::chrono_literals;

ClientWorkerImpl::ClientWorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                                   Envoy::Upstream::ClusterManagerPtr& cluster_manager,
                                   const BenchmarkClientFactory& benchmark_client_factory,
                                   const TerminationPredicateFactory& termination_predicate_factory,
                                   const SequencerFactory& sequencer_factory,
                                   const RequestSourceFactory& request_generator_factory,
                                   Envoy::Stats::Store& store, const int worker_number,
                                   const Envoy::MonotonicTime starting_time,
                                   Envoy::Tracing::HttpTracerPtr& http_tracer)
    : WorkerImpl(api, tls, store), termination_predicate_factory_(termination_predicate_factory),
      sequencer_factory_(sequencer_factory), worker_scope_(store_.createScope("cluster.")),
      worker_number_scope_(worker_scope_->createScope(fmt::format("{}.", worker_number))),
      worker_number_(worker_number), starting_time_(starting_time), http_tracer_(http_tracer),
      request_generator_(request_generator_factory.create()),
      benchmark_client_(benchmark_client_factory.create(
          api, *dispatcher_, *worker_number_scope_, cluster_manager, http_tracer_,
          fmt::format("{}", worker_number), *request_generator_)) {}

void ClientWorkerImpl::simpleWarmup() {
  if (time_source_.monotonicTime() - starting_time_ > 50us) {
    ENVOY_LOG(error, "phase starting too late - {} ns delta",
              (time_source_.monotonicTime() - starting_time_).count());
  }
  while (starting_time_ > time_source_.monotonicTime()) {
    dispatcher_->run(Envoy::Event::Dispatcher::RunType::NonBlock);
  }
  TerminationPredicatePtr warmup_predicates =
      termination_predicate_factory_.create(time_source_, *worker_number_scope_);
  warmup_predicates->appendToChain(
      std::make_unique<DurationTerminationPredicateImpl>(time_source_, 1s));
  phases_.emplace_back(std::make_unique<PhaseImpl>(
      "warmup", sequencer_factory_.create(time_source_, *dispatcher_, *benchmark_client_,
                                          std::move(warmup_predicates), *worker_number_scope_)));
  phases_.back()->run();
}

void ClientWorkerImpl::work() {
  simpleWarmup();
  if (worker_number_scope_->counter("sequencer.failed_terminations").value() == 0) {
    benchmark_client_->setMeasureLatencies(true);
    phases_.emplace_back(std::make_unique<PhaseImpl>(
        "main", sequencer_factory_.create(
                    time_source_, *dispatcher_, *benchmark_client_,
                    termination_predicate_factory_.create(time_source_, *worker_number_scope_),
                    *worker_number_scope_)));
    phases_.back()->run();
  }

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
  auto& sequencer = phases_.back()->sequencer();
  StatisticPtrMap s2 = sequencer.statistics();
  statistics.insert(s1.begin(), s1.end());
  statistics.insert(s2.begin(), s2.end());
  return statistics;
}

} // namespace Client
} // namespace Nighthawk