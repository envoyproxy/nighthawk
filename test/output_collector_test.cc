#include "external/envoy/test/test_common/simulated_time_system.h"

#include "source/client/options_impl.h"
#include "source/client/output_collector_impl.h"

#include "test/client/utility.h"
#include "test/test_common/proto_matchers.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {
namespace {

using ::google::protobuf::TextFormat;
using ::nighthawk::client::UserDefinedOutput;

class OutputCollectorTest : public Test, public Envoy::Event::TestUsingSimulatedTime {
public:
  OutputCollectorTest() {}
};

TEST_F(OutputCollectorTest, AddResultCanAddUserDefinedOutputs) {
  std::unique_ptr<OptionsImpl> options =
      TestUtility::createOptionsImpl("foo https://unresolved.host/");
  OutputCollectorImpl collector(simTime(), *options);

  std::map<std::string, uint64_t> empty_map{};
  std::vector<nighthawk::client::UserDefinedOutput> user_defined_outputs{};
  std::chrono::nanoseconds execution_duration = std::chrono::nanoseconds::zero();

  UserDefinedOutput output1;
  TextFormat::ParseFromString(R"pb(name: "nighthawk.fake_user_defined_output"
    typed_config {
      [type.googleapis.com/nighthawk.FakeUserDefinedOutput] {worker_name: "test_worker"}
    }
  )pb",
                              &output1);
  UserDefinedOutput output2;
  TextFormat::ParseFromString(R"pb(name: "nighthawk.fake_user_defined_output"
    typed_config {
      [type.googleapis.com/google.protobuf.StringValue] {value: "my_test_value"}
    }
  )pb",
                              &output2);
  user_defined_outputs.push_back(output1);
  user_defined_outputs.push_back(output2);

  collector.addResult(/*name = */ "worker_1",
                      /*statistics=*/{},
                      /*counters=*/empty_map, execution_duration,
                      /*first_acquisition_time=*/absl::nullopt, user_defined_outputs);

  nighthawk::client::Output full_output = collector.toProto();
  EXPECT_EQ(full_output.results_size(), 1);
  EXPECT_EQ(full_output.results(0).user_defined_outputs_size(), 2);
  EXPECT_THAT(full_output.results(0).user_defined_outputs(0), EqualsProto(output1));
  EXPECT_THAT(full_output.results(0).user_defined_outputs(1), EqualsProto(output2));
}

TEST_F(OutputCollectorTest, AddResultWorksWithNoUserDefinedOutputs) {
  std::unique_ptr<OptionsImpl> options =
      TestUtility::createOptionsImpl("foo https://unresolved.host/");
  OutputCollectorImpl collector(simTime(), *options);

  std::map<std::string, uint64_t> empty_map{};
  std::vector<nighthawk::client::UserDefinedOutput> user_defined_outputs{};
  std::chrono::nanoseconds execution_duration = std::chrono::nanoseconds::zero();

  collector.addResult(/*name = */ "worker_1",
                      /*statistics=*/{},
                      /*counters=*/empty_map, execution_duration,
                      /*first_acquisition_time=*/absl::nullopt, user_defined_outputs);

  nighthawk::client::Output full_output = collector.toProto();
  EXPECT_EQ(full_output.results_size(), 1);
  EXPECT_EQ(full_output.results(0).user_defined_outputs_size(), 0);
}

} // namespace
} // namespace Client
} // namespace Nighthawk
