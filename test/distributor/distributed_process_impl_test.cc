#include <vector>

#include "external/envoy/test/test_common/simulated_time_system.h"
#include "external/envoy/test/test_common/utility.h"

#include "api/distributor/distributor_mock.grpc.pb.h"
#include "api/sink/sink_mock.grpc.pb.h"

#include "source/client/options_impl.h"
#include "source/client/output_collector_impl.h"
#include "source/distributor/distributed_process_impl.h"

#include "grpcpp/test/mock_stream.h"

#include "test/client/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::Envoy::Protobuf::util::MessageDifferencer;
using ::grpc::testing::MockClientReaderWriter;
using ::nighthawk::DistributedRequest;
using ::nighthawk::DistributedResponse;
using ::nighthawk::SinkRequest;
using ::nighthawk::SinkResponse;
using ::Nighthawk::Client::TestUtility;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;

class DistributedProcessImplTest : public testing::Test,
                                   public Envoy::Event::TestUsingSimulatedTime {
protected:
  nighthawk::MockNighthawkDistributorStub distributor_stub;
  nighthawk::MockNighthawkSinkStub sink_stub;
  Client::OptionsPtr options;
};

TEST_F(DistributedProcessImplTest, InitDistributedExecutionAndQuerySink) {
  // In the regular flow, we expect two calls to the mock distributor stub we pass
  // in to the DistributedProcessImpl when calling run():
  // - One to initiate execution.
  // - One to query the sink afterwards.
  // When that finishes, we test expectations for execution-id's and sink-result handling.
  options =
      TestUtility::createOptionsImpl("foo --sink bar:443 https://foo/ --services service1:444");
  DistributedProcessImpl process(*options, distributor_stub, sink_stub);
  Client::OutputCollectorImpl collector(simTime(), *options);
  DistributedRequest observed_init_request;
  SinkRequest observed_sink_request;
  nighthawk::client::Output sink_output;
  nighthawk::client::Counter* sink_foo_counter = sink_output.add_results()->add_counters();
  sink_foo_counter->set_name("foo");
  sink_foo_counter->set_value(33);

  EXPECT_CALL(distributor_stub, DistributedRequestStreamRaw)
      .WillOnce([&observed_init_request](grpc::ClientContext*) {
        auto* mock_reader_writer =
            new MockClientReaderWriter<DistributedRequest, DistributedResponse>();
        DistributedResponse dictated_response;
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillOnce(DoAll(SetArgPointee<0>(dictated_response), Return(true)))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillOnce(DoAll(SaveArg<0>(&observed_init_request), Return(true)));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish()).WillOnce(Return(grpc::Status::OK));
        return mock_reader_writer;
      });
  EXPECT_CALL(sink_stub, SinkRequestStreamRaw)
      .WillOnce([&observed_sink_request, sink_output](grpc::ClientContext*) {
        SinkResponse dictated_response;
        dictated_response.mutable_execution_response()->mutable_output()->MergeFrom(sink_output);
        auto* mock_reader_writer = new MockClientReaderWriter<SinkRequest, SinkResponse>();
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillOnce(DoAll(SetArgPointee<0>(dictated_response), Return(true)))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillOnce(DoAll(SaveArg<0>(&observed_sink_request), Return(true)));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish()).WillOnce(Return(grpc::Status::OK));
        return mock_reader_writer;
      });

  EXPECT_TRUE(process.run(collector));
  ASSERT_TRUE(observed_init_request.has_execution_request());
  ASSERT_TRUE(observed_init_request.execution_request().has_start_request());
  ASSERT_TRUE(observed_init_request.execution_request().start_request().has_options());
  const std::string execution_id =
      observed_init_request.execution_request().start_request().options().execution_id().value();
  EXPECT_NE(execution_id, "");
  EXPECT_EQ(observed_sink_request.execution_id(), execution_id);
  EXPECT_TRUE(MessageDifferencer::Equivalent(collector.toProto(), sink_output));

  process.shutdown();
}

TEST_F(DistributedProcessImplTest, WriteFailureOnDistributorLoadTestInitiations) {
  options = TestUtility::createOptionsImpl("foo --sink bar:443 https://foo/");
  DistributedProcessImpl process(*options, distributor_stub, sink_stub);
  Client::OutputCollectorImpl collector(simTime(), *options);
  DistributedRequest observed_init_request;

  EXPECT_CALL(distributor_stub, DistributedRequestStreamRaw)
      .WillOnce([&observed_init_request](grpc::ClientContext*) {
        // Simulate a write failure on the load test initiation request.
        auto* mock_reader_writer =
            new MockClientReaderWriter<DistributedRequest, DistributedResponse>();
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillOnce(DoAll(SaveArg<0>(&observed_init_request), Return(false)));
        return mock_reader_writer;
      });

  EXPECT_FALSE(process.run(collector));
  process.shutdown();
}

TEST_F(DistributedProcessImplTest, WriteFailureOnSinkRequest) {
  options = TestUtility::createOptionsImpl("foo --sink bar:443 https://foo/");
  DistributedProcessImpl process(*options, distributor_stub, sink_stub);
  Client::OutputCollectorImpl collector(simTime(), *options);
  DistributedRequest observed_init_request;
  SinkRequest observed_sink_request;

  EXPECT_CALL(distributor_stub, DistributedRequestStreamRaw)
      .WillOnce([&observed_init_request](grpc::ClientContext*) {
        auto* mock_reader_writer =
            new MockClientReaderWriter<DistributedRequest, DistributedResponse>();
        DistributedResponse dictated_response;
        EXPECT_CALL(*mock_reader_writer, Read(_))
            .WillOnce(DoAll(SetArgPointee<0>(dictated_response), Return(true)))
            .WillOnce(Return(false));
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillOnce(DoAll(SaveArg<0>(&observed_init_request), Return(true)));
        EXPECT_CALL(*mock_reader_writer, WritesDone()).WillOnce(Return(true));
        EXPECT_CALL(*mock_reader_writer, Finish()).WillOnce(Return(grpc::Status::OK));
        return mock_reader_writer;
      });

  EXPECT_CALL(sink_stub, SinkRequestStreamRaw)
      .WillOnce([&observed_sink_request](grpc::ClientContext*) {
        // Simulate a write failure on the sink request.
        auto* mock_reader_writer = new MockClientReaderWriter<SinkRequest, SinkResponse>();
        EXPECT_CALL(*mock_reader_writer, Write(_, _))
            .WillOnce(DoAll(SaveArg<0>(&observed_sink_request), Return(false)));
        return mock_reader_writer;
      });

  EXPECT_FALSE(process.run(collector));
  process.shutdown();
}

TEST_F(DistributedProcessImplTest, NoSinkConfigurationResultsInFailure) {
  // Not specifying a sink configuration should fail, at least today.
  options = TestUtility::createOptionsImpl("foo https://foo/");
  Client::OutputCollectorImpl collector(simTime(), *options);
  DistributedProcessImpl process(*options, distributor_stub, sink_stub);
  EXPECT_FALSE(process.run(collector));
}

TEST_F(DistributedProcessImplTest, RequestExecutionCancellationDoesNotCrash) {
  // This call isn't supported yet, and issues a log warning up usage. We don't expect great things
  // from it, just that it doesn't crash, even when called at an inappropriate time like here where
  // when we call it the process has not even had run() called on it.
  options = TestUtility::createOptionsImpl("foo --sink bar:443 https://foo/");
  DistributedProcessImpl process(*options, distributor_stub, sink_stub);
  process.requestExecutionCancellation();
}

} // namespace
} // namespace Nighthawk
