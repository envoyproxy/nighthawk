#include "common/worker_impl.h"

#include "envoy/runtime/runtime.h"
#include "envoy/thread_local/thread_local.h"

using namespace std::chrono_literals;

namespace Nighthawk {

WorkerImpl::WorkerImpl(Envoy::Api::Api& api, Envoy::ThreadLocal::Instance& tls,
                       Envoy::Stats::Store& store)
    : thread_factory_(api.threadFactory()), dispatcher_(api.allocateDispatcher()), tls_(tls),
      store_(store), time_source_(api.timeSource()), file_system_(api.fileSystem()) {
  tls.registerThread(*dispatcher_, false);
}

WorkerImpl::~WorkerImpl() {
  started_ = false;
  tls_.shutdownThread();
  thread_->join();
}

void WorkerImpl::start() {
  ASSERT(!started_ && !completed_);
  started_ = true;
  thread_ = thread_factory_.createThread([this]() {
    ASSERT(Envoy::Runtime::LoaderSingleton::getExisting() != nullptr);
    // Run the dispatcher to let the callbacks posted by registerThread() execute.
    dispatcher_->run(Envoy::Event::Dispatcher::RunType::NonBlock);
    work();
    completed_ = true;
    while (started_) {
      usleep(1000 * 250);
    }
  });
}

void WorkerImpl::waitForCompletion() {
  ASSERT(started_ && !completed_);
  // thread_->join();
  while (!completed_) {
    usleep(1000 * 250);
  }
}

} // namespace Nighthawk
