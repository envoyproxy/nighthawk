#include <thread>

#include "common/api/api_impl.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/runtime/runtime_impl.h"
#include "common/stats/isolated_store_impl.h"
#include "common/worker_impl.h"

#include "test/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/test_common/thread_factory_for_test.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {

class TestWorker : public WorkerImpl {
public:
  TestWorker(Envoy::Api::Impl& api, Envoy::ThreadLocal::Instance& tls)
      : WorkerImpl(api, tls, std::make_unique<Envoy::Stats::IsolatedStoreImpl>()),
        thread_id_(std::this_thread::get_id()) {}
  void work() override {
    EXPECT_NE(thread_id_, std::this_thread::get_id());
    ran_ = true;
  }

  bool ran_{};
  std::thread::id thread_id_;
};

class WorkerTest : public Test {
public:
  WorkerTest() : api_(Envoy::Thread::threadFactoryForTest(), store_, time_system_, file_system_) {}

  Envoy::Filesystem::InstanceImplPosix file_system_;
  Envoy::Api::Impl api_;
  Envoy::ThreadLocal::MockInstance tls_;
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Event::TestRealTimeSystem time_system_;
  Envoy::Runtime::RandomGeneratorImpl rand_;
};

TEST_F(WorkerTest, WorkerExecutesOnThread) {
  InSequence in_sequence;

  EXPECT_CALL(tls_, registerThread(_, false)).Times(1);
  EXPECT_CALL(tls_, allocateSlot()).Times(1);

  TestWorker worker(api_, tls_);
  NiceMock<Envoy::Event::MockDispatcher> dispatcher;
  Envoy::Runtime::ScopedLoaderSingleton loader(Envoy::Runtime::LoaderPtr{
      new Envoy::Runtime::LoaderImpl(dispatcher, tls_, {}, "test-cluster", store_, rand_, api_)});

  worker.start();
  worker.waitForCompletion();

  EXPECT_CALL(tls_, shutdownThread()).Times(1);
  ASSERT_TRUE(worker.ran_);
}

} // namespace Nighthawk
