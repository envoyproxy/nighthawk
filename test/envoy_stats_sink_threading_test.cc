// Guards the threading contract Nighthawk has to satisfy to host Envoy's own stats sinks, which
// resolve a thread local writer on every use. Both cases mirror ProcessImpl: worker threads
// register before the main thread does, the sink is created on the main thread afterwards, and it
// is then used from a worker thread - including for a final flush after global thread local
// threading has been shut down, which is the order ProcessImpl::shutdown() uses.
//
// The precondition is that every thread touching such a sink is registered with the ThreadLocal
// instance; an unregistered thread trips ASSERT(currentThreadRegisteredWorker(index)) in
// SlotImpl::getWorker(). Nighthawk's worker threads register in WorkerImpl's constructor and the
// main thread registers in ProcessImpl, so the runtime satisfies it; a unit test constructing such
// a sink without registering threads would not.
#include <thread>

#include "absl/synchronization/notification.h"

#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy/source/common/network/utility.h"
#include "external/envoy/source/common/stats/allocator_impl.h"
#include "external/envoy/source/common/stats/thread_local_store.h"
#include "external/envoy/source/common/thread_local/thread_local_impl.h"
#include "external/envoy/source/extensions/stat_sinks/common/statsd/statsd.h"
#include "external/envoy/test/mocks/event/mocks.h"
#include "external/envoy/test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Nighthawk {

using namespace testing;

TEST(EnvoyStatsSinkThreading, UdpStatsdSinkUnderAWorkerThread) {
  Envoy::Api::ApiPtr api = Envoy::Api::createApiForTest();
  Envoy::ThreadLocal::InstanceImpl tls;
  Envoy::Event::DispatcherPtr main_dispatcher = api->allocateDispatcher("main_thread");

  // A worker registers before the main thread does, as ClientWorkerImpl does via createWorkers().
  Envoy::Event::DispatcherPtr worker_dispatcher = api->allocateDispatcher("worker_thread");
  tls.registerThread(*worker_dispatcher, false);
  tls.registerThread(*main_dispatcher, true);

  Envoy::Stats::SymbolTableImpl symbol_table;
  Envoy::Stats::AllocatorImpl allocator(symbol_table);
  Envoy::Stats::ThreadLocalStoreImpl store(allocator);
  store.initializeThreading(*main_dispatcher, tls);

  // Sink creation happens on the main thread, after both registrations, as in setupStatsSinks().
  Envoy::Network::Address::InstanceConstSharedPtr address =
      Envoy::Network::Utility::parseInternetAddressNoThrow("127.0.0.1", 8125);
  ASSERT_NE(nullptr, address);
  auto sink = std::make_unique<Envoy::Extensions::StatSinks::Common::Statsd::UdpStatsdSink>(
      tls, address, /*use_tag=*/true, "nighthawk");
  store.addSink(*sink);

  // The worker thread runs its dispatcher once (WorkerImpl::start does RunType::NonBlock) and then
  // uses the sink, which is where the thread local writer is resolved.
  std::string failure;
  std::thread worker([&]() {
    worker_dispatcher->run(Envoy::Event::Dispatcher::RunType::NonBlock);
    Envoy::Stats::Histogram& histogram = store.rootScope()->histogramFromString(
        "test_histogram", Envoy::Stats::Histogram::Unit::Unspecified);
    histogram.recordValue(1500);
    // recordValue() reaches the sink through deliverHistogramToSinks(), which is where
    // UdpStatsdSink resolves its thread local writer - the same resolution flush() performs.
  });
  worker.join();

  main_dispatcher->run(Envoy::Event::Dispatcher::RunType::NonBlock);
  tls.shutdownGlobalThreading();
  store.shutdownThreading();
  sink.reset();
  tls.shutdownThread();
  EXPECT_EQ("", failure);
}

// ProcessImpl::shutdown() calls tls_.shutdownGlobalThreading() before flush_worker_->shutdown(),
// and FlushWorkerImpl::shutdownThread() performs a final flush. This exercises that order: a sink
// used from a worker thread after global thread local threading has been shut down.
TEST(EnvoyStatsSinkThreading, SinkUsedAfterGlobalThreadingShutdown) {
  Envoy::Api::ApiPtr api = Envoy::Api::createApiForTest();
  Envoy::ThreadLocal::InstanceImpl tls;
  Envoy::Event::DispatcherPtr main_dispatcher = api->allocateDispatcher("main_thread");
  Envoy::Event::DispatcherPtr worker_dispatcher = api->allocateDispatcher("worker_thread");
  tls.registerThread(*worker_dispatcher, false);
  tls.registerThread(*main_dispatcher, true);

  Envoy::Stats::SymbolTableImpl symbol_table;
  Envoy::Stats::AllocatorImpl allocator(symbol_table);
  Envoy::Stats::ThreadLocalStoreImpl store(allocator);
  store.initializeThreading(*main_dispatcher, tls);

  Envoy::Network::Address::InstanceConstSharedPtr address =
      Envoy::Network::Utility::parseInternetAddressNoThrow("127.0.0.1", 8125);
  ASSERT_NE(nullptr, address);
  auto sink = std::make_unique<Envoy::Extensions::StatSinks::Common::Statsd::UdpStatsdSink>(
      tls, address, /*use_tag=*/true, "nighthawk");
  store.addSink(*sink);

  Envoy::Stats::Histogram& histogram = store.rootScope()->histogramFromString(
      "test_histogram", Envoy::Stats::Histogram::Unit::Unspecified);

  // One long lived worker thread, as FlushWorkerImpl is: it runs its dispatcher (picking up the
  // thread local initializers), then waits, then performs a final flush on the same thread after
  // global threading has been shut down.
  absl::Notification registered;
  absl::Notification globals_shut_down;
  std::thread worker([&]() {
    worker_dispatcher->run(Envoy::Event::Dispatcher::RunType::NonBlock);
    histogram.recordValue(1500);
    registered.Notify();
    globals_shut_down.WaitForNotification();
    // FlushWorkerImpl::shutdownThread() flushes here, after ProcessImpl shut global threading down.
    histogram.recordValue(2500);
    tls.shutdownThread();
  });
  registered.WaitForNotification();
  main_dispatcher->run(Envoy::Event::Dispatcher::RunType::NonBlock);

  // The shutdown order ProcessImpl uses today.
  tls.shutdownGlobalThreading();
  globals_shut_down.Notify();
  worker.join();

  store.shutdownThreading();
  sink.reset();
  tls.shutdownThread();
}

} // namespace Nighthawk
