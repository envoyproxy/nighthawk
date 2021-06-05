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

#include "source/client/flush_worker_impl.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace Client {
namespace {

using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::Test;

// Number of times the simulated timer loops run in simulateTimerLoop().
const int kNumTimerLoops = 100;

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
            *dispatcher_, tls_, {}, local_info, store_, rand, validation_visitor, api_)});
    sink_ = new StrictMock<Envoy::Stats::MockSink>();
    stats_sinks_.emplace_back(sink_);

    EXPECT_CALL(api_, threadFactory()).WillRepeatedly(ReturnRef(thread_factory_));
    // api_ takes the ownership of dispatcher_.
    EXPECT_CALL(api_, allocateDispatcher_(_, _)).WillOnce(Return(dispatcher_));
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
    EXPECT_CALL(*dispatcher_, exit()).WillOnce(Invoke([&]() {}));
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

  // Simulate the periodical timer which runs kNumTimerLoops iterations before signaling another
  // thread to call dispatcher->exit().
  void simulateTimerLoop() {
    int loop_iterations = 0;
    do {
      if (timer_set_) {
        timer_set_ = false;
        timer_cb_();
      }
      if (++loop_iterations == kNumTimerLoops) {
        signal_dispatcher_to_exit_.set_value();
        return;
      }
    } while (true);
  }

  NiceMock<Envoy::Api::MockApi> api_;
  Envoy::Thread::ThreadFactory& thread_factory_;
  Envoy::Stats::IsolatedStoreImpl store_;
  NiceMock<Envoy::ThreadLocal::MockInstance> tls_;
  // owned by FlushWorkerImpl's dispatcher member variable.
  NiceMock<Envoy::Event::MockDispatcher>* dispatcher_ = nullptr;
  std::unique_ptr<Envoy::Runtime::ScopedLoaderSingleton> loader_;

  // owned by FlushWorkerImpl's stat_flush_timer_ member variable.
  NiceMock<Envoy::Event::MockTimer>* timer_;
  Envoy::Event::TimerCb timer_cb_;
  bool timer_set_{}; // used to simulate whether the timer is enabled.
  std::promise<void> signal_dispatcher_to_exit_;

  Envoy::Stats::MockSink* sink_ = nullptr; // owned by stats_sinks_
  std::list<std::unique_ptr<Envoy::Stats::Sink>> stats_sinks_;
};

// Verify stats are flushed periodically until dispatcher->exit() is called from
// another thread.
TEST_F(FlushWorkerTest, WorkerFlushStatsPeriodically) {
  std::chrono::milliseconds stats_flush_interval{10};
  setupDispatcherTimerEmulation();

  FlushWorkerImpl worker(stats_flush_interval, api_, tls_, store_, stats_sinks_);

  std::thread thread = std::thread([&worker, this] {
    // Wait for the while loop to run kNumTimerLoops times in simulateTimerLoop().
    signal_dispatcher_to_exit_.get_future().wait();
    worker.exitDispatcher();
  });

  expectDispatcherRun();
  // Check flush() is called kNumTimerLoops times in simulateTimerLoop().
  EXPECT_CALL(*sink_, flush(_)).Times(kNumTimerLoops);

  worker.start();
  worker.waitForCompletion();
  thread.join();
  // Stats flush should happen exactly once as the final flush is done in
  // FlushWorkerImpl::shutdownThread().
  EXPECT_CALL(*sink_, flush(_));
  worker.shutdown();
}

// Verify the final flush is always done in FlushWorkerImpl::shutdownThread()
// even when the dispatcher and timer is not set up (expectDispatcherRun() is not called).
TEST_F(FlushWorkerTest, FinalFlush) {
  std::chrono::milliseconds stats_flush_interval{10};

  FlushWorkerImpl worker(stats_flush_interval, api_, tls_, store_, stats_sinks_);

  worker.start();
  worker.waitForCompletion();
  // Stats flush should happen exactly once as the final flush is done in
  // FlushWorkerImpl::shutdownThread().
  EXPECT_CALL(*sink_, flush(_));
  worker.shutdown();
}

} // namespace
} // namespace Client
} // namespace Nighthawk
