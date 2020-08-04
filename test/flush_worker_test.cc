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

using namespace testing;

class FlushWorkerTest : public Test {
public:
  FlushWorkerTest() : thread_factory_(Envoy::Thread::threadFactoryForTest()) {
    dispatcher_ = new NiceMock<Envoy::Event::MockDispatcher>();
    loader_ = std::make_unique<Envoy::Runtime::ScopedLoaderSingleton>(
        Envoy::Runtime::LoaderPtr{new Envoy::Runtime::LoaderImpl(
            *dispatcher_, tls_, {}, local_info_, store_, rand_, validation_visitor_, mock_api_)});
    sink_ = new StrictMock<Envoy::Stats::MockSink>();
    stats_sinks_.emplace_back(sink_);

    EXPECT_CALL(mock_api_, threadFactory()).WillRepeatedly(ReturnRef(thread_factory_));
    EXPECT_CALL(mock_api_, allocateDispatcher_(_, _)).WillOnce(Return(dispatcher_));
  }

  // We set up emulating timer firing and moving simulated time forward in
  // simulateTimerloop() below.
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
    EXPECT_CALL(*dispatcher_, exit()).WillOnce(Invoke([&]() { stopped_ = true; }));
  }

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

  void simulateTimerLoop() {
    while (!stopped_) {
      if (timer_set_) {
        timer_set_ = false;
        timer_cb_();
      }
    }
  }

  NiceMock<Envoy::Api::MockApi> mock_api_;
  Envoy::Thread::ThreadFactory& thread_factory_;
  Envoy::Stats::IsolatedStoreImpl store_;
  NiceMock<Envoy::ThreadLocal::MockInstance> tls_;
  Envoy::Random::RandomGeneratorImpl rand_;
  NiceMock<Envoy::Event::MockDispatcher>* dispatcher_ = nullptr;
  std::unique_ptr<Envoy::Runtime::ScopedLoaderSingleton> loader_;
  NiceMock<Envoy::LocalInfo::MockLocalInfo> local_info_;
  NiceMock<Envoy::ProtobufMessage::MockValidationVisitor> validation_visitor_;

  NiceMock<Envoy::Event::MockTimer>* timer_; // not owned
  Envoy::Event::TimerCb timer_cb_;
  bool timer_set_{};
  bool stopped_{};

  Envoy::Stats::MockSink* sink_ = nullptr;
  std::list<std::unique_ptr<Envoy::Stats::Sink>> stats_sinks_;
  std::chrono::milliseconds stats_flush_interval_{10};
};

TEST_F(FlushWorkerTest, WorkerFlushStatsPeriodically) {
  setupDispatcherTimerEmulation();

  auto worker = std::make_unique<FlushWorkerImpl>(mock_api_, tls_, store_, stats_sinks_,
                                                  stats_flush_interval_);

  std::thread thread = std::thread([&worker] {
    sleep(1);
    worker->exitDispatcher();
  });

  expectDispatcherRun();
  // During the 1s sleep, timer_cb_ in simulateTimerLoop() should be called at
  // least 100 times.
  EXPECT_CALL(*sink_, flush(_)).Times(testing::AtLeast(100));

  worker->start();
  worker->waitForCompletion();
  thread.join();
  worker->shutdown();
}

TEST_F(FlushWorkerTest, FinalFlush) {
  auto worker = std::make_unique<FlushWorkerImpl>(mock_api_, tls_, store_, stats_sinks_,
                                                  stats_flush_interval_);

  // Final flush should be done in FlushWorkerImpl::shutdownThread().
  EXPECT_CALL(*sink_, flush(_)).Times(1);

  worker->start();
  worker->waitForCompletion();
  worker->shutdown();
}

} // namespace Client
} // namespace Nighthawk
