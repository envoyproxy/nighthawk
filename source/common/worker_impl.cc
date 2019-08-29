#include "common/worker_impl.h"

#include "envoy/runtime/runtime.h"
#include "envoy/thread_local/thread_local.h"

namespace Nighthawk {

WorkerImpl::WorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                       Envoy::Stats::Store& store)
    : thread_factory_(api.threadFactory()), dispatcher_(api.allocateDispatcher()), tls_(tls),
      store_(store), time_source_(api.timeSource()), exit_lock_guard_(exit_lock_) {
  tls.registerThread(*dispatcher_, false);
}

WorkerImpl::~WorkerImpl() { shutDown(); }

void WorkerImpl::notifyExit() {
  // code analysis isn't able to understand what we are doing here.
  // suppress the errors from that locally, so we can continue to use
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wthread-safety-analysis"
#endif
  exit_lock_guard_.release();
#ifdef __clang__
#pragma clang diagnostic pop
#endif
}

void WorkerImpl::shutDown() {
  tls_.shutdownThread();
  // Unblock the associated worker thread, and wait for it to wrap up.
  notifyExit();
  if (future_.valid()) {
    future_.wait();
  }
}

void WorkerImpl::start() {
  RELEASE_ASSERT(!started_, "WorkerImpl::start() expected started_ to be false");
  started_ = true;
  Envoy::Thread::LockGuard completion_lock(completion_lock_);
  Envoy::Thread::CondVar wait_event;
  future_ = std::future<void>(std::async(std::launch::async, [this, &wait_event]() {
    RELEASE_ASSERT(Envoy::Runtime::LoaderSingleton::getExisting() != nullptr,
                   "Couldn't get runtime");
    {
      Envoy::Thread::LockGuard completion_lock(completion_lock_);
      wait_event.notifyOne();
      // Run the dispatcher to let the callbacks posted by registerThread() execute.
      dispatcher_->run(Envoy::Event::Dispatcher::RunType::NonBlock);
      work();
    }
    // This will block untill shutdown is called.
    Envoy::Thread::LockGuard exit_lock(exit_lock_);
  }));
  // The next expected call from the caller is waitForCompletion(), which needs the completion lock
  // to be held by the worker thread, so we wait for that before returning control.
  wait_event.wait(completion_lock_);
}

void WorkerImpl::waitForCompletion() { Envoy::Thread::LockGuard completion_lock(completion_lock_); }

} // namespace Nighthawk