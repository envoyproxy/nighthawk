#pragma once

#include "envoy/api/api.h"
#include "envoy/common/time.h"
#include "envoy/filesystem/filesystem.h"
#include "envoy/stats/store.h"
#include "envoy/thread/thread.h"

#include "nighthawk/common/worker.h"

#include "external/envoy/source/common/common/logger.h"

namespace Nighthawk {

class WorkerImpl : virtual public Worker {
public:
  WorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls, Envoy::Stats::Store& store);
  ~WorkerImpl() override;

  void start() override;
  void waitForCompletion() override;

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
  Envoy::Filesystem::Instance& file_system_;

private:
  Envoy::Thread::ThreadPtr thread_;
  bool started_{};
  bool completed_{};
};

} // namespace Nighthawk