#include "source/client/flush_worker_impl.h"

#include "external/envoy/source/common/stats/symbol_table.h"
#include "external/envoy/source/server/server.h"

#include "source/common/utility.h"

namespace Nighthawk {
namespace Client {

FlushWorkerImpl::FlushWorkerImpl(const std::chrono::milliseconds& stats_flush_interval,
                                 Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                                 Envoy::Stats::Store& store,
                                 std::list<std::unique_ptr<Envoy::Stats::Sink>>& stats_sinks,
                                 Envoy::Upstream::ClusterManager& cluster_manager)
    : WorkerImpl(api, tls, store), stats_flush_interval_(stats_flush_interval),
      cluster_manager_(cluster_manager) {
  for (auto& sink : stats_sinks) {
    stats_sinks_.emplace_back(std::move(sink));
  }
}

void FlushWorkerImpl::work() {
  stat_flush_timer_ = dispatcher_->createTimer([this]() -> void { flushStats(); });
  stat_flush_timer_->enableTimer(stats_flush_interval_);
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::RunUntilExit);
}

void FlushWorkerImpl::shutdownThread() {
  if (stat_flush_timer_ != nullptr) {
    stat_flush_timer_->disableTimer();
    stat_flush_timer_.reset();
  }
  // Make the final flush before the flush worker gets shutdown.
  flushStats();
}

void FlushWorkerImpl::flushStats() {
  // Create a snapshot and flush to all sinks. Even if there are no sinks,
  // creating the snapshot has the important property that it latches all counters on a periodic
  // basis.
  Envoy::Server::MetricSnapshotImpl snapshot(store_, cluster_manager_, time_source_);
  for (std::unique_ptr<Envoy::Stats::Sink>& sink : stats_sinks_) {
    sink->flush(snapshot);
  }
  if (stat_flush_timer_ != nullptr) {
    stat_flush_timer_->enableTimer(stats_flush_interval_);
  }
}

} // namespace Client
} // namespace Nighthawk
