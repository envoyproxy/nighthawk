#include <functional>
#include <thread>

#include "envoy/upstream/cluster_manager.h"

#include "external/envoy/source/common/common/random_generator.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/test/mocks/local_info/mocks.h"
#include "external/envoy/test/mocks/protobuf/mocks.h"
#include "external/envoy/test/mocks/thread_local/mocks.h"
#include "external/envoy/test/test_common/simulated_time_system.h"

#include "source/client/client_worker_impl.h"
#include "source/common/statistic_impl.h"

#include "test/mocks/client/mock_benchmark_client.h"
#include "test/mocks/client/mock_benchmark_client_factory.h"
#include "test/mocks/common/mock_request_source.h"
#include "test/mocks/common/mock_request_source_factory.h"
#include "test/mocks/common/mock_sequencer.h"
#include "test/mocks/common/mock_sequencer_factory.h"
#include "test/mocks/common/mock_termination_predicate.h"
#include "test/mocks/common/mock_termination_predicate_factory.h"

#include "gtest/gtest.h"

using namespace testing;
using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

class ClientWorkerTest : public Test {
public:
  ClientWorkerTest()
      : api_(Envoy::Api::createApiForTest()), thread_id_(std::this_thread::get_id()) {
    loader_ = std::make_unique<Envoy::Runtime::ScopedLoaderSingleton>(
        Envoy::Runtime::LoaderPtr{new Envoy::Runtime::LoaderImpl(
            dispatcher_, tls_, {}, local_info_, store_, rand_, validation_visitor_, *api_)});
    benchmark_client_ = new MockBenchmarkClient();
    sequencer_ = new MockSequencer();
    request_generator_ = new MockRequestSource();

    EXPECT_CALL(benchmark_client_factory_, create(_, _, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(Return(ByMove(std::unique_ptr<BenchmarkClient>(benchmark_client_))));

    EXPECT_CALL(sequencer_factory_, create(_, _, _, _, _, _))
        .Times(1)
        .WillOnce(Return(ByMove(std::unique_ptr<Sequencer>(sequencer_))));

    EXPECT_CALL(request_generator_factory_, create(_, _, _, _))
        .Times(1)
        .WillOnce(Return(ByMove(std::unique_ptr<RequestSource>(request_generator_))));
    EXPECT_CALL(*request_generator_, initOnThread());

    EXPECT_CALL(termination_predicate_factory_, create(_, _, _))
        .WillOnce(Return(ByMove(createMockTerminationPredicate())));
  }

  StatisticPtrMap createStatisticPtrMap() const {
    StatisticPtrMap map;
    map["foo1"] = &statistic_;
    map["foo2"] = &statistic_;
    return map;
  }

  bool CheckThreadChanged(const CompletionCallback&) {
    EXPECT_NE(thread_id_, std::this_thread::get_id());
    return false;
  }

  TerminationPredicatePtr createMockTerminationPredicate() {
    auto predicate = std::make_unique<NiceMock<MockTerminationPredicate>>();
    ON_CALL(*predicate, appendToChain(_)).WillByDefault(ReturnRef(*predicate));
    return predicate;
  }

  StreamingStatistic statistic_;
  Envoy::Api::ApiPtr api_;
  std::thread::id thread_id_;
  MockBenchmarkClientFactory benchmark_client_factory_;
  MockTerminationPredicateFactory termination_predicate_factory_;
  MockSequencerFactory sequencer_factory_;
  MockRequestSourceFactory request_generator_factory_;
  Envoy::Stats::IsolatedStoreImpl store_;
  NiceMock<Envoy::ThreadLocal::MockInstance> tls_;
  Envoy::Event::SimulatedTimeSystem time_system_;
  MockBenchmarkClient* benchmark_client_;
  MockSequencer* sequencer_;
  MockRequestSource* request_generator_;
  Envoy::Random::RandomGeneratorImpl rand_;
  NiceMock<Envoy::Event::MockDispatcher> dispatcher_;
  std::unique_ptr<Envoy::Runtime::ScopedLoaderSingleton> loader_;
  NiceMock<Envoy::LocalInfo::MockLocalInfo> local_info_;
  NiceMock<Envoy::ProtobufMessage::MockValidationVisitor> validation_visitor_;
  Envoy::Upstream::ClusterManagerPtr cluster_manager_ptr_;
  Envoy::Tracing::HttpTracerSharedPtr http_tracer_;
};

TEST_F(ClientWorkerTest, BasicTest) {
  ASSERT_EQ(std::this_thread::get_id(), thread_id_);

  {
    InSequence dummy;
    EXPECT_CALL(*benchmark_client_, setShouldMeasureLatencies(false));
    EXPECT_CALL(*benchmark_client_, tryStartRequest(_))
        .WillOnce(Invoke(this, &ClientWorkerTest::CheckThreadChanged));
    EXPECT_CALL(*benchmark_client_, setShouldMeasureLatencies(true));
    EXPECT_CALL(*sequencer_, start);
    EXPECT_CALL(*sequencer_, waitForCompletion);
    EXPECT_CALL(*benchmark_client_, terminate());
  }
  int worker_number = 12345;

  auto worker = std::make_unique<ClientWorkerImpl>(
      *api_, tls_, cluster_manager_ptr_, benchmark_client_factory_, termination_predicate_factory_,
      sequencer_factory_, request_generator_factory_, store_, worker_number,
      time_system_.monotonicTime(), http_tracer_, ClientWorkerImpl::HardCodedWarmupStyle::ON);

  worker->start();
  worker->waitForCompletion();

  EXPECT_CALL(*benchmark_client_, statistics()).WillOnce(Return(createStatisticPtrMap()));
  EXPECT_CALL(*sequencer_, statistics()).WillOnce(Return(createStatisticPtrMap()));

  auto statistics = worker->statistics();
  EXPECT_EQ(2, statistics.size());
  worker->shutdown();
}

} // namespace Client
} // namespace Nighthawk
