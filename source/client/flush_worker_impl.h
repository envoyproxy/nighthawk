// Flush worker implementation. Flush worker periodically flushes metrics
// snapshot to all configured stats sinks in Nighthawk.
#pragma once

#include <vector>

#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/stats/sink.h"
#include "envoy/stats/store.h"
#include "envoy/thread_local/thread_local.h"

#include "source/common/worker_impl.h"

namespace Nighthawk {
namespace Client {

// Only a single live flush worker instance can be created in Nighthawk at any
// time.
// Flush worker periodically flushes metrics snapshot to all configured stats sinks in Nighthawk. It
// will keep running until exitDispatcher() gets called after all client workers are completed in
// process_impl.cc. It will make the last flush before shutdown in shutdownThread().
class FlushWorkerImpl : public WorkerImpl {
public:
  // Constructor to call parent class's constructor and initialize member
  // variables stats_sinks_ and stats_flush_interval_.
  // @param stats_flush_interval time interval between each flush.
  // @param api supplies the Api instance for WorkerImpl's constructor. See
  // envoy/include/envoy/api/api.h for its definition.
  // @param tls supplies the ThreadLocal::Instance for WorkerImpl's constructor.
  // See envoy/include/envoy/thread_local/thread_local.h for its definition.
  // @param store supplies the stats store instance for WorkerImpl's constructor.
  // @param stats_sinks list of configured stats sinks where the stats will be
  // flushed to.
  FlushWorkerImpl(const std::chrono::milliseconds& stats_flush_interval, Envoy::Api::Api& api,
                  Envoy::ThreadLocal::Instance& tls, Envoy::Stats::Store& store,
                  std::list<std::unique_ptr<Envoy::Stats::Sink>>& stats_sinks);

  void shutdownThread() override;

  // exitDispatcher() stops the dispatcher and the flush timer running in flush worker. It must be
  // called after all client workers are completed in process_impl.cc to make sure all metrics will
  // be flushed.
  void exitDispatcher() { dispatcher_->exit(); }

protected:
  void work() override;

private:
  // Flush the stats sinks. Note: stats flushing may not be synchronous, depending on each stat
  // sink's implementation. Therefore, this function may return prior to flushing taking place.
  void flushStats();

  std::list<std::unique_ptr<Envoy::Stats::Sink>> stats_sinks_;
  const std::chrono::milliseconds stats_flush_interval_;
  Envoy::Event::TimerPtr stat_flush_timer_;
};

} // namespace Client
} // namespace Nighthawk
