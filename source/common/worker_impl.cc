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

WorkerImpl::~WorkerImpl() { RELEASE_ASSERT(shutdown_, "Call shutdown() before destruction."); }

void WorkerImpl::shutdown() {
  shutdown_ = true;
  signal_thread_to_exit_.set_value();
  thread_.join();
}

void WorkerImpl::start() {
  RELEASE_ASSERT(!started_, "WorkerImpl::start() expected started_ to be false");
  started_ = true;
  shutdown_ = false;
  thread_ = std::thread([this]() {
    RELEASE_ASSERT(Envoy::Runtime::LoaderSingleton::getExisting() != nullptr,
                   "Couldn't get runtime");
    dispatcher_->run(Envoy::Event::Dispatcher::RunType::NonBlock);
    work();
    complete_.set_value();
    signal_thread_to_exit_.get_future().wait();
    shutdownThread();
    tls_.shutdownThread();
  });
}

void WorkerImpl::waitForCompletion() { complete_.get_future().wait(); }

} // namespace Nighthawk
