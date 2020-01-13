#include <vector>

#include "nighthawk/common/exception.h"

#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/utility.h"

#include "common/uri_impl.h"

#include "client/options_impl.h"
#include "client/output_collector_impl.h"
#include "client/process_impl.h"

#include "test/client/utility.h"
#include "test/mocks.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

// TODO(https://github.com/envoyproxy/nighthawk/issues/179): Mock Process in client_test, and move
// it's tests in here. Note: these tests do not have a backend set up to talk to. That's why we
// expect failure.
class ProcessTest : public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  enum class RunExpectation { EXPECT_SUCCESS, EXPECT_FAILURE };

  ProcessTest()
      : loopback_address_(Envoy::Network::Test::getLoopbackAddressUrlString(GetParam())),
        options_(TestUtility::createOptionsImpl(
            fmt::format("foo --duration 1 -v error --rps 10 https://{}/", loopback_address_))){};
  void runProcess(RunExpectation expectation) {
    ProcessPtr process = std::make_unique<ProcessImpl>(*options_, time_system_);
    OutputCollectorImpl collector(time_system_, *options_);
    const auto result =
        process->run(collector) ? RunExpectation::EXPECT_SUCCESS : RunExpectation::EXPECT_FAILURE;
    EXPECT_EQ(result, expectation);
    process->shutdown();
  }

  void checkSniHostComputation(const std::vector<std::string>& uris,
                               const std::vector<std::string>& request_headers,
                               const Envoy::Http::Protocol protocol,
                               absl::string_view expected_sni_host) {
    std::vector<UriPtr> parsed_uris;
    parsed_uris.reserve(uris.size());
    for (const std::string& uri : uris) {
      parsed_uris.push_back(std::make_unique<UriImpl>(uri));
    }
    std::string sni_host = ProcessImpl::computeSniHost(parsed_uris, request_headers, protocol);
    EXPECT_EQ(sni_host, expected_sni_host);
  }

  const std::string loopback_address_;
  OptionsPtr options_;
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ProcessTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(ProcessTest, TwoProcessInSequence) {
  runProcess(RunExpectation::EXPECT_FAILURE);
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --h2 --duration 1 --rps 10 https://{}/", loopback_address_));
  runProcess(RunExpectation::EXPECT_FAILURE);
}

// TODO(oschaaf): move to python int. tests once it adds to coverage.
TEST_P(ProcessTest, BadTracerSpec) {
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --trace foo://localhost:79/api/v1/spans https://{}/", loopback_address_));
  runProcess(RunExpectation::EXPECT_FAILURE);
}

TEST_P(ProcessTest, SniHostComputation) {
  const std::vector<Envoy::Http::Protocol> all_protocols{
      Envoy::Http::Protocol::Http10, Envoy::Http::Protocol::Http11, Envoy::Http::Protocol::Http2,
      Envoy::Http::Protocol::Http3};
  for (const auto protocol : all_protocols) {
    checkSniHostComputation({"localhost"}, {}, protocol, "localhost");
    checkSniHostComputation({"localhost:81"}, {}, protocol, "localhost");
    checkSniHostComputation({"localhost"}, {"Host: foo"}, protocol, "foo");
    checkSniHostComputation({"localhost:81"}, {"Host: foo"}, protocol, "foo");
    const std::string expected_sni_host(protocol >= Envoy::Http::Protocol::Http2 ? "foo"
                                                                                 : "localhost");
    checkSniHostComputation({"localhost"}, {":authority: foo"}, protocol, expected_sni_host);
    checkSniHostComputation({"localhost:81"}, {":authority: foo"}, protocol, expected_sni_host);
  }
}

} // namespace Client
} // namespace Nighthawk
