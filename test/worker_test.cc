#include <thread>

#include "gtest/gtest.h"

#include "common/api/api_impl.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/runtime/runtime_impl.h"
#include "common/stats/isolated_store_impl.h"

#include "test/mocks/thread_local/mocks.h"

#include "nighthawk/test/mocks.h"

#include "nighthawk/source/common/worker_impl.h"

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

class WorkerTest : public testing::Test {
public:
  WorkerTest()
      : api_(Envoy::Thread::ThreadFactorySingleton::get(), store_, time_system_, file_system_) {}

  Envoy::Filesystem::InstanceImplPosix file_system_;
  Envoy::Api::Impl api_;
  Envoy::ThreadLocal::MockInstance tls_;
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Event::RealTimeSystem time_system_;
  Envoy::Runtime::RandomGeneratorImpl rand_;
};

TEST_F(WorkerTest, WorkerExecutesOnThread) {
  ::testing::InSequence in_sequence;

  EXPECT_CALL(tls_, registerThread(_, false)).Times(1);
  EXPECT_CALL(tls_, allocateSlot()).Times(1);

  TestWorker worker(api_, tls_);
  Envoy::Runtime::ScopedLoaderSingleton loader(
      Envoy::Runtime::LoaderPtr{new Envoy::Runtime::LoaderImpl(rand_, store_, tls_)});
  worker.start();
  worker.waitForCompletion();

  EXPECT_CALL(tls_, shutdownThread()).Times(1);
  ASSERT_TRUE(worker.ran_);
}

} // namespace Nighthawk
