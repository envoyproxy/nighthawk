#include <functional>
#include <thread>

#include "external/envoy/source/common/common/random_generator.h"
#include "external/envoy/source/common/event/dispatcher_impl.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/test/mocks/api/mocks.h"
#include "external/envoy/test/mocks/local_info/mocks.h"
#include "external/envoy/test/mocks/protobuf/mocks.h"
#include "external/envoy/test/mocks/stats/mocks.h"
#include "external/envoy/test/mocks/thread_local/mocks.h"
#include "external/envoy/test/test_common/simulated_time_system.h"

#include "client/flush_worker_impl.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace Client {
namespace {

using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::Test;

class FlushWorkerTest : public Test {
public:
  FlushWorkerTest()
      : thread_factory_(Envoy::Thread::threadFactoryForTest()),
        dispatcher_(new NiceMock<Envoy::Event::MockDispatcher>()) {
    Envoy::Random::RandomGeneratorImpl rand;
    NiceMock<Envoy::LocalInfo::MockLocalInfo> local_info;
    NiceMock<Envoy::ProtobufMessage::MockValidationVisitor> validation_visitor;
    loader_ = std::make_unique<Envoy::Runtime::ScopedLoaderSingleton>(
        Envoy::Runtime::LoaderPtr{new Envoy::Runtime::LoaderImpl(
            *dispatcher_, tls_, {}, local_info, store_, rand, validation_visitor, mock_api_)});
    sink_ = new StrictMock<Envoy::Stats::MockSink>();
    stats_sinks_.emplace_back(sink_);

    EXPECT_CALL(mock_api_, threadFactory()).WillRepeatedly(ReturnRef(thread_factory_));
    // mock_api_ takes the ownership of dispatcher_.
    EXPECT_CALL(mock_api_, allocateDispatcher_(_, _)).WillOnce(Return(dispatcher_));
  }

  // Set up emulating timer firing and corresponding expectations and behaviors.
  void setupDispatcherTimerEmulation() {
    timer_ = new NiceMock<Envoy::Event::MockTimer>();
    EXPECT_CALL(*dispatcher_, createTimer_(_)).WillOnce(Invoke([&](Envoy::Event::TimerCb cb) {
      timer_cb_ = std::move(cb);
      return timer_;
    }));
    EXPECT_CALL(*timer_, enableTimer(_, _))
        .WillRepeatedly(Invoke([&](const std::chrono::microseconds,
                                   const Envoy::ScopeTrackedObject*) { timer_set_ = true; }));
    EXPECT_CALL(*timer_, disableTimer()).WillOnce(Invoke([&]() { timer_set_ = false; }));
    EXPECT_CALL(*dispatcher_, exit()).WillOnce(Invoke([&]() {
      auto guard = std::make_unique<Envoy::Thread::LockGuard>(lock_);
      stopped_ = true;
    }));
  }

  // Set up expected behaviors when run() is called on dispatcher_.
  void expectDispatcherRun() {
    EXPECT_CALL(*dispatcher_, run(_))
        .WillOnce(Invoke([&](Envoy::Event::DispatcherImpl::RunType type) {
          // The first dispatcher run is in WorkerImpl::start().
          ASSERT_EQ(Envoy::Event::DispatcherImpl::RunType::NonBlock, type);
        }))
        .WillOnce(Invoke([&](Envoy::Event::DispatcherImpl::RunType type) {
          // The second dispatcher run is in FlushWorkerImpl::work().
          ASSERT_EQ(Envoy::Event::DispatcherImpl::RunType::RunUntilExit, type);
          simulateTimerLoop();
        }));
  }

  // Simulate the periodical timer which keeps running until dispatcher->exit()
  // is called from another thread. The std::promise signal_dispatcher_to_exit_
  // is used here to guarantee the while loop runs for at least 100 times before dispatcher->exit()
  // is called.
  void simulateTimerLoop() {
    int i = 0;
    while (true) {
      i++;
      // At least run the while loop for 100 times.
      if (i == 100) {
        signal_dispatcher_to_exit_.set_value();
      }
      auto guard = std::make_unique<Envoy::Thread::LockGuard>(lock_);
      if (stopped_) {
        break;
      } else if (timer_set_) {
        timer_set_ = false;
        timer_cb_();
      }
    }
  }

  NiceMock<Envoy::Api::MockApi> mock_api_;
  Envoy::Thread::ThreadFactory& thread_factory_;
  Envoy::Stats::IsolatedStoreImpl store_;
  NiceMock<Envoy::ThreadLocal::MockInstance> tls_;
  NiceMock<Envoy::Event::MockDispatcher>* dispatcher_ = nullptr; // not owned
  std::unique_ptr<Envoy::Runtime::ScopedLoaderSingleton> loader_;

  NiceMock<Envoy::Event::MockTimer>* timer_; // not owned
  Envoy::Event::TimerCb timer_cb_;
  bool timer_set_{};
  bool stopped_{false};
  // Protect stopped_.
  Envoy::Thread::MutexBasicLockable lock_;
  std::promise<void> signal_dispatcher_to_exit_;

  Envoy::Stats::MockSink* sink_ = nullptr; // owned by stats_sinks_
  std::list<std::unique_ptr<Envoy::Stats::Sink>> stats_sinks_;
};

// Verify stats are flushed periodically until dispatcher->exit() is called from
// another thread.
TEST_F(FlushWorkerTest, WorkerFlushStatsPeriodically) {
  std::chrono::milliseconds stats_flush_interval{10};
  setupDispatcherTimerEmulation();

  FlushWorkerImpl worker(mock_api_, tls_, store_, stats_sinks_, stats_flush_interval);

  std::thread thread = std::thread([&worker, this] {
    // Wait for the while loop run at least 100 times in simulateTimerLoop().
    signal_dispatcher_to_exit_.get_future().wait();
    worker.exitDispatcher();
  });

  expectDispatcherRun();
  // Check flush() is called at least 100 times in simulateTimerLoop().
  EXPECT_CALL(*sink_, flush(_)).Times(testing::AtLeast(100));

  worker.start();
  worker.waitForCompletion();
  thread.join();
  worker.shutdown();
}

// Verify the final flush is always done in FlushWorkerImpl::shutdownThread()
// even when the dispatcher and timer is not set up (expectDispatcherRun() is not called).
TEST_F(FlushWorkerTest, FinalFlush) {
  std::chrono::milliseconds stats_flush_interval{10};

  FlushWorkerImpl worker(mock_api_, tls_, store_, stats_sinks_, stats_flush_interval);

  worker.start();
  worker.waitForCompletion();
  // Stats flush should happen exactly once as the final flush is done in
  // FlushWorkerImpl::shutdownThread().
  EXPECT_CALL(*sink_, flush(_)).Times(1);
  worker.shutdown();
}

} // namespace
} // namespace Client
} // namespace Nighthawk
