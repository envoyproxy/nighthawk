#include "client/client_worker_impl.h"

#include "external/envoy/source/common/stats/symbol_table_impl.h"

#include "common/cached_time_source_impl.h"
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
                                   Envoy::Tracing::HttpTracerSharedPtr& http_tracer,
                                   const HardCodedWarmupStyle hardcoded_warmup_style)
    : WorkerImpl(api, tls, store),
      time_source_(std::make_unique<CachedTimeSourceImpl>(*dispatcher_)),
      termination_predicate_factory_(termination_predicate_factory),
      sequencer_factory_(sequencer_factory), worker_scope_(store_.createScope("cluster.")),
      worker_number_scope_(worker_scope_->createScope(fmt::format("{}.", worker_number))),
      worker_number_(worker_number), http_tracer_(http_tracer),
      request_generator_(
          request_generator_factory.create(cluster_manager, *dispatcher_, *worker_number_scope_,
                                           fmt::format("{}.requestsource", worker_number))),
      benchmark_client_(benchmark_client_factory.create(
          api, *dispatcher_, *worker_number_scope_, cluster_manager, http_tracer_,
          fmt::format("{}", worker_number), worker_number, *request_generator_)),
      phase_(
          std::make_unique<PhaseImpl>("main",
                                      sequencer_factory_.create(
                                          *time_source_, *dispatcher_,
                                          [this](CompletionCallback f) -> bool {
                                            return benchmark_client_->tryStartRequest(std::move(f));
                                          },
                                          termination_predicate_factory_.create(
                                              *time_source_, *worker_number_scope_, starting_time),
                                          *worker_number_scope_, starting_time),
                                      true)),
      hardcoded_warmup_style_(hardcoded_warmup_style) {}

void ClientWorkerImpl::simpleWarmup() {
  ENVOY_LOG(debug, "> worker {}: warmup start.", worker_number_);
  if (benchmark_client_->tryStartRequest([this](bool, bool) { dispatcher_->exit(); })) {
    dispatcher_->run(Envoy::Event::Dispatcher::RunType::RunUntilExit);
  } else {
    ENVOY_LOG(warn, "> worker {}: failed to initiate warmup request.", worker_number_);
  }
  ENVOY_LOG(debug, "> worker {}: warmup done.", worker_number_);
}

void ClientWorkerImpl::work() {
  benchmark_client_->setShouldMeasureLatencies(false);
  request_generator_->initOnThread();
  if (hardcoded_warmup_style_ == HardCodedWarmupStyle::ON) {
    simpleWarmup();
  }
  benchmark_client_->setShouldMeasureLatencies(phase_->shouldMeasureLatencies());
  phase_->run();

  // Save a final snapshot of the worker-specific counter accumulations before
  // we exit the thread.
  for (const auto& stat : store_.counters()) {
    // First, we strip the cluster prefix
    std::string stat_name = std::string(absl::StripPrefix(stat->name(), "cluster."));
    stat_name = std::string(absl::StripPrefix(stat_name, "worker."));
    // Second, we strip our own prefix if it's there, else we skip.
    const std::string worker_prefix = fmt::format("{}.", worker_number_);
    if (stat->value() && absl::StartsWith(stat_name, worker_prefix)) {
      threadLocalCounterValues_[std::string(absl::StripPrefix(stat_name, worker_prefix))] =
          stat->value();
    }
  }
  // Note that benchmark_client_ is not terminated here, but in shutdownThread() below. This is to
  // to prevent the shutdown artifacts from influencing the test result counters. The main thread
  // still needs to be able to read the counters for reporting the global numbers, and those
  // should be consistent.
}

void ClientWorkerImpl::shutdownThread() { benchmark_client_->terminate(); }

void ClientWorkerImpl::requestExecutionCancellation() {
  // We just bump a counter, which is watched by a static termination predicate.
  // A useful side effect is that this counter will propagate to the output, which leaves
  // a note about that execution was subject to cancellation.
  dispatcher_->post(
      [this]() { worker_number_scope_->counterFromString("graceful_stop_requested").inc(); });
}

StatisticPtrMap ClientWorkerImpl::statistics() const {
  StatisticPtrMap statistics;
  StatisticPtrMap s1 = benchmark_client_->statistics();
  Sequencer& sequencer = phase_->sequencer();
  StatisticPtrMap s2 = sequencer.statistics();
  statistics.insert(s1.begin(), s1.end());
  statistics.insert(s2.begin(), s2.end());
  return statistics;
}

} // namespace Client
} // namespace Nighthawk
