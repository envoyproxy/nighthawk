#include <functional>
#include <thread>

#include "external/envoy/source/common/api/api_impl.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/test/mocks/init/mocks.h"
#include "external/envoy/test/mocks/local_info/mocks.h"
#include "external/envoy/test/mocks/protobuf/mocks.h"
#include "external/envoy/test/mocks/thread_local/mocks.h"
#include "external/envoy/test/test_common/thread_factory_for_test.h"

#include "common/filesystem/filesystem_impl.h" // XXX(oschaaf):
#include "common/statistic_impl.h"
#include "common/uri_impl.h"

#include "client/client_worker_impl.h"

#include "test/mocks.h"

#include "envoy/upstream/cluster_manager.h"
#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

class ClientWorkerTest : public Test {
public:
  ClientWorkerTest()
      : api_(Envoy::Thread::threadFactoryForTest(), store_, time_system_, file_system_),
        thread_id_(std::this_thread::get_id()) {
    loader_ = std::make_unique<Envoy::Runtime::ScopedLoaderSingleton>(Envoy::Runtime::LoaderPtr{
        new Envoy::Runtime::LoaderImpl(dispatcher_, tls_, {}, local_info_, init_manager_, store_,
                                       rand_, validation_visitor_, api_)});
    benchmark_client_ = new MockBenchmarkClient();
    sequencer_ = new MockSequencer();

    EXPECT_CALL(benchmark_client_factory_, create(_, _, _, _, _))
        .Times(1)
        .WillOnce(Return(ByMove(std::unique_ptr<BenchmarkClient>(benchmark_client_))));

    EXPECT_CALL(sequencer_factory_, create(_, _, _, _))
        .Times(1)
        .WillOnce(Return(ByMove(std::unique_ptr<Sequencer>(sequencer_))));
  }

  StatisticPtrMap createStatisticPtrMap() const {
    StatisticPtrMap map;
    map["foo1"] = &statistic_;
    map["foo2"] = &statistic_;
    return map;
  }

  bool CheckThreadChanged(const std::function<void()>&) {
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
  NiceMock<Envoy::ThreadLocal::MockInstance> tls_;
  Envoy::Event::TestRealTimeSystem time_system_;
  MockBenchmarkClient* benchmark_client_;
  MockSequencer* sequencer_;
  Envoy::Runtime::RandomGeneratorImpl rand_;
  NiceMock<Envoy::Event::MockDispatcher> dispatcher_;
  Envoy::Filesystem::InstanceImplPosix file_system_;
  std::unique_ptr<Envoy::Runtime::ScopedLoaderSingleton> loader_;
  NiceMock<Envoy::LocalInfo::MockLocalInfo> local_info_;
  Envoy::Init::MockManager init_manager_;
  NiceMock<Envoy::ProtobufMessage::MockValidationVisitor> validation_visitor_;
  Envoy::Upstream::ClusterManagerPtr cluster_manager_ptr_;
};

TEST_F(ClientWorkerTest, BasicTest) {
  ASSERT_EQ(std::this_thread::get_id(), thread_id_);

  {
    InSequence dummy;

    EXPECT_CALL(*sequencer_, start).Times(1);
    EXPECT_CALL(*sequencer_, waitForCompletion).Times(1);
  }

  {
    InSequence dummy;

    // warmup
    EXPECT_CALL(*benchmark_client_, prefetchPoolConnections()).Times(1);
    EXPECT_CALL(*benchmark_client_, tryStartOne(_))
        .Times(1)
        .WillRepeatedly(Invoke(this, &ClientWorkerTest::CheckThreadChanged));

    // latency measurement will be initiated
    EXPECT_CALL(*benchmark_client_, setMeasureLatencies(true)).Times(1);
    EXPECT_CALL(*benchmark_client_, terminate()).Times(1);
  }

  int worker_number = 12345;
  auto worker = std::make_unique<ClientWorkerImpl>(
      api_, tls_, cluster_manager_ptr_, benchmark_client_factory_, sequencer_factory_,
      std::make_unique<Nighthawk::UriImpl>("http://foo"), store_, worker_number,
      time_system_.monotonicTime(), true);

  worker->start();
  worker->waitForCompletion();

  EXPECT_CALL(*benchmark_client_, statistics()).Times(1).WillOnce(Return(createStatisticPtrMap()));
  EXPECT_CALL(*sequencer_, statistics()).Times(1).WillOnce(Return(createStatisticPtrMap()));

  auto statistics = worker->statistics();
  EXPECT_EQ(2, statistics.size());
}

} // namespace Client
} // namespace Nighthawk
