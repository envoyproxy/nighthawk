#include <grpc++/grpc++.h>

#include "nighthawk/common/exception.h"

#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/utility.h"

#include "source/client/service_main.h"

#include "gtest/gtest.h"

using namespace testing;

// TODO(oschaaf): this gets us some coverage, but we need more functional testing.
// See if we can add some python integration tests.

namespace Nighthawk {
namespace Client {

class ServiceMainTest : public Test {};

TEST_F(ServiceMainTest, BadArgs) {
  std::vector<const char*> argv = {"foo", "bar"};
  EXPECT_THROW(ServiceMain(argv.size(), argv.data()), std::exception);
}

TEST_F(ServiceMainTest, BadHost) {
  std::vector<const char*> argv = {"foo", "--listen", "b|-%ar"};
  ServiceMain service_main(argv.size(), argv.data());
  EXPECT_THROW(service_main.start(), NighthawkException);
}

TEST_F(ServiceMainTest, UnkownHost) {
  std::vector<const char*> argv = {"foo", "--listen", "bar"};
  ServiceMain service_main(argv.size(), argv.data());
  EXPECT_THROW(service_main.start(), NighthawkException);
}

TEST_F(ServiceMainTest, NoArgs) {
  std::vector<const char*> argv = {"foo"};
  ServiceMain service(argv.size(), argv.data());
  service.start();
  service.shutdown();
}

TEST_F(ServiceMainTest, Unbindable) {
  const std::string dest = fmt::format("unknownhost:10");
  std::vector<const char*> argv = {"foo", "--listen", dest.c_str()};
  ServiceMain service_main(argv.size(), argv.data());
  EXPECT_THROW(service_main.start(), NighthawkException);
}

class ServiceMainTestP : public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  ServiceMainTestP()
      : loopback_address_(Envoy::Network::Test::getLoopbackAddressUrlString(GetParam())) {}

  const std::string loopback_address_;
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ServiceMainTestP,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(ServiceMainTestP, OnlyIp) {
  const std::string dest = fmt::format("{}", loopback_address_);
  std::vector<const char*> argv = {"foo", "--listen", dest.c_str()};
  ServiceMain service(argv.size(), argv.data());
  service.start();
  service.shutdown();
}

TEST_P(ServiceMainTestP, PortZero) {
  // We should be able to bind to port 0
  const std::string dest = fmt::format("{}:0", loopback_address_);
  std::vector<const char*> argv = {"foo", "--listen", dest.c_str()};
  ServiceMain service_main(argv.size(), argv.data());
  EXPECT_NO_THROW(service_main.start());
  service_main.shutdown();
}

} // namespace Client
} // namespace Nighthawk
