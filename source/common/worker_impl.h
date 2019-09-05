#pragma once

#include <thread>

#include "envoy/api/api.h"
#include "envoy/common/time.h"
#include "envoy/stats/store.h"

#include "nighthawk/common/worker.h"

#include "external/envoy/source/common/common/lock_guard.h"
#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/common/thread.h"

namespace Nighthawk {

class WorkerImpl : virtual public Worker, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  WorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls, Envoy::Stats::Store& store);
  ~WorkerImpl() override;

  void start() override;
  void waitForCompletion() override;
  void shutdown() override;

protected:
  /**
   * Perform the actual work on the associated thread initiated by start().
   */
  virtual void work() PURE;

  Envoy::Thread::ThreadFactory& thread_factory_;
  Envoy::Event::DispatcherPtr dispatcher_;
  Envoy::ThreadLocal::Instance& tls_;
  Envoy::Stats::Store& store_;
  Envoy::TimeSource& time_source_;

private:
  void notifyExit();
  Envoy::Thread::MutexBasicLockable exit_lock_;
  std::unique_ptr<Envoy::Thread::LockGuard> exit_lock_guard_;
  Envoy::Thread::MutexBasicLockable completion_lock_;
  std::thread thread_;
  bool started_{};
  bool shutdown_{true};
};

} // namespace Nighthawk
