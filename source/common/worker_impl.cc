#include "common/worker_impl.h"

#include "envoy/runtime/runtime.h"
#include "envoy/thread_local/thread_local.h"

namespace Nighthawk {

WorkerImpl::WorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                       Envoy::Stats::Store& store)
    : thread_factory_(api.threadFactory()), dispatcher_(api.allocateDispatcher()), tls_(tls),
      store_(store), time_source_(api.timeSource()) {
  tls.registerThread(*dispatcher_, false);
}

WorkerImpl::~WorkerImpl() { shutDown(); }

void WorkerImpl::notifyExit() {
  // Unblock the associated worker thread, and wait for it to wrap up.
  // Note: thread-safety-analysis would flag this, because it isn't able to
  // determine that we hold the lock. So we use a unique_ptr instead
  // of ReleasableLockGuard on purpose here, to hide this from clang-tidy, in favor of adding
  // suppressions.
  exit_lock_guard_.reset();
}

void WorkerImpl::shutDown() {
  tls_.shutdownThread();
  notifyExit();
  if (thread_.joinable()) {
    thread_.join();
  }
}

void WorkerImpl::start() {
  RELEASE_ASSERT(!started_, "WorkerImpl::start() expected started_ to be false");
  started_ = true;
  exit_lock_guard_ = std::make_unique<Envoy::Thread::LockGuard>(exit_lock_);
  Envoy::Thread::LockGuard completion_lock(completion_lock_);
  Envoy::Thread::CondVar wait_event;
  thread_ = std::thread([this, &wait_event]() {
    RELEASE_ASSERT(Envoy::Runtime::LoaderSingleton::getExisting() != nullptr,
                   "Couldn't get runtime");
    {
      Envoy::Thread::LockGuard completion_lock(completion_lock_);
      // Indicate that we now hold completion_lock.
      wait_event.notifyOne();
      // Run the dispatcher to let the callbacks posted by registerThread() execute.
      dispatcher_->run(Envoy::Event::Dispatcher::RunType::NonBlock);
      work();
    }
    // This will block until shutdown is called.
    Envoy::Thread::LockGuard exit_lock(exit_lock_);
  });
  // The next expected call from the caller is waitForCompletion(), which needs the completion lock
  // to be held by the worker thread, so we wait for that before returning control.
  wait_event.wait(completion_lock_);
}

void WorkerImpl::waitForCompletion() { Envoy::Thread::LockGuard completion_lock(completion_lock_); }

} // namespace Nighthawk