#include "gtest/gtest.h"

#include <functional>

#include <thread>

#include "common/api/api_impl.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/runtime/runtime_impl.h"
#include "common/stats/isolated_store_impl.h"

#include "test/mocks/thread_local/mocks.h"

#include "nighthawk/test/mocks.h"

#include "nighthawk/source/client/client_worker_impl.h"
#include "nighthawk/source/common/statistic_impl.h"

namespace Nighthawk {
namespace Client {

class ClientWorkerTest : public testing::Test {
public:
  ClientWorkerTest()
      : api_(Envoy::Thread::ThreadFactorySingleton::get(), store_, time_system_, file_system_),
        thread_id_(std::this_thread::get_id()),
        loader_(Envoy::Runtime::LoaderPtr{new Envoy::Runtime::LoaderImpl(rand_, store_, tls_)}) {
    benchmark_client_ = new MockBenchmarkClient();
    sequencer_ = new MockSequencer();

    EXPECT_CALL(benchmark_client_factory_, create(_, _, _, _))
        .Times(1)
        .WillOnce(testing::Return(
            testing::ByMove(std::unique_ptr<MockBenchmarkClient>(benchmark_client_))));

    EXPECT_CALL(sequencer_factory_, create(_, _, _, _))
        .Times(1)
        .WillOnce(testing::Return(testing::ByMove(std::unique_ptr<MockSequencer>(sequencer_))));
  }

  StatisticPtrMap createStatisticPtrMap() const {
    StatisticPtrMap map;
    map["foo1"] = &statistic_;
    map["foo2"] = &statistic_;
    return map;
  }

  bool CheckTreadChanged(std::function<void()>) {
    EXPECT_NE(thread_id_, std::this_thread::get_id());
    return true;
  }

  StreamingStatistic statistic_;
  Envoy::Api::Impl api_;
  std::thread::id thread_id_;
  MockOptions options_;
  MockBenchmarkClientFactory benchmark_client_factory_;
  MockSequencerFactory sequencer_factory_;
  Envoy::Stats::IsolatedStoreImpl store_;
  ::testing::NiceMock<Envoy::ThreadLocal::MockInstance> tls_;
  Envoy::Event::RealTimeSystem time_system_;
  MockBenchmarkClient* benchmark_client_;
  MockSequencer* sequencer_;
  Envoy::Runtime::RandomGeneratorImpl rand_;
  Envoy::Runtime::ScopedLoaderSingleton loader_;
  Envoy::Filesystem::InstanceImplPosix file_system_;
};

TEST_F(ClientWorkerTest, BasicTest) {
  ASSERT_EQ(std::this_thread::get_id(), thread_id_);

  {
    testing::InSequence dummy;

    EXPECT_CALL(*benchmark_client_, initialize).Times(1);
    EXPECT_CALL(*sequencer_, start).Times(1);
    EXPECT_CALL(*sequencer_, waitForCompletion).Times(1);
  }

  {
    testing::InSequence dummy;

    // warmup
    EXPECT_CALL(*benchmark_client_, tryStartOne(_))
        .Times(1)
        .WillRepeatedly(Invoke(this, &ClientWorkerTest::CheckTreadChanged));

    // latency measurement will be initiated
    EXPECT_CALL(*benchmark_client_, setMeasureLatencies(true)).Times(1);
    EXPECT_CALL(*benchmark_client_, terminate()).Times(1);
  }

  int worker_number = 12345;
  auto worker = std::make_unique<ClientWorkerImpl>(
      api_, tls_, benchmark_client_factory_, sequencer_factory_, Uri::Parse("http://foo"),
      std::make_unique<Envoy::Stats::IsolatedStoreImpl>(), worker_number,
      time_system_.monotonicTime());

  worker->start();
  worker->waitForCompletion();

  EXPECT_CALL(*benchmark_client_, statistics())
      .Times(1)
      .WillOnce(testing::Return(createStatisticPtrMap()));
  EXPECT_CALL(*sequencer_, statistics())
      .Times(1)
      .WillOnce(testing::Return(createStatisticPtrMap()));

  auto statistics = worker->statistics();
  EXPECT_EQ(2, statistics.size());
}

} // namespace Client
} // namespace Nighthawk
