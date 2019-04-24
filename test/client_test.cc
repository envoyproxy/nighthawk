#include <chrono>

#include "gtest/gtest.h"

#include "test/mocks/event/mocks.h"
#include "test/mocks/stats/mocks.h"

#include "test/integration/http_integration.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"
#include "test/server/utility.h"
#include "test/test_common/utility.h"

#include "common/api/api_impl.h"
#include "common/common/thread_impl.h"
#include "common/filesystem/filesystem_impl.h"

#include "test/client/utility.h"
#include "test/mocks.h"

#include "client/client.h"
#include "client/factories_impl.h"
#include "client/options_impl.h"

#include "test/server/int_server.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

class ClientServerTest : public TestWithParam<Envoy::Network::Address::IpVersion>,
                         Envoy::Logger::Loggable<Envoy::Logger::Id::testing> {
public:
  ClientServerTest()
      : transport_socket_factory_(), ip_version_(GetParam()),
        listening_socket_(
            Envoy::Network::Utility::parseInternetAddressAndPort(fmt::format(
                "{}:{}", Envoy::Network::Test::getLoopbackAddressUrlString(ip_version_), 0)),
            nullptr, true),
        server_("server", listening_socket_, transport_socket_factory_,
                Envoy::Http::CodecClient::Type::HTTP1) {}

  std::string testUrl() {
    const std::string address = Envoy::Network::Test::getLoopbackAddressUrlString(GetParam());
    uint16_t port = static_cast<uint16_t>(listening_socket_.localAddress()->ip()->port());
    return fmt::format("http://{}:{}/", address, port);
  }

  const char* getAddressFamilyOptionString() {
    auto ip_version = GetParam();
    RELEASE_ASSERT(ip_version == Envoy::Network::Address::IpVersion::v4 ||
                       ip_version == Envoy::Network::Address::IpVersion::v6,
                   "bad ip version");
    return ip_version == Envoy::Network::Address::IpVersion::v6 ? "v6" : "v4";
  }

protected:
  Envoy::Network::RawBufferSocketFactory transport_socket_factory_;
  Envoy::Network::Address::IpVersion ip_version_;
  Envoy::Network::TcpListenSocket listening_socket_;
  Mixer::Integration::Server server_;
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ClientServerTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(ClientServerTest, NormalRun) {
  Mixer::Integration::ServerCallbackHelper server_callbacks; // sends a 200 OK to everything
  Main program(Nighthawk::Client::TestUtility::createOptionsImpl(
      fmt::format("foo --address-family {} --duration 2 --rps 10 {}",
                  getAddressFamilyOptionString(), testUrl())));
  server_.start(server_callbacks);
  EXPECT_TRUE(program.run());
  EXPECT_EQ(server_callbacks.connectionsAccepted(), 1);
  EXPECT_GE(server_callbacks.requestsReceived(), 10);
  // Server does not close its own sockets but instead relies on the client to initate the close
  EXPECT_EQ(0, server_callbacks.localCloses());
  // Server sees a client-initiated close for every socket it accepts
  EXPECT_EQ(server_callbacks.remoteCloses(), server_callbacks.connectionsAccepted());
}

TEST_P(ClientServerTest, Prefetching) {
  Mixer::Integration::ServerCallbackHelper server_callbacks; // sends a 200 OK to everything
  Main program(Nighthawk::Client::TestUtility::createOptionsImpl(fmt::format(
      "foo --address-family {} --duration 2 --prefetch-connections --connections 25 --rps 10 {}",
      getAddressFamilyOptionString(), testUrl())));
  server_.start(server_callbacks);
  EXPECT_TRUE(program.run());
  EXPECT_EQ(server_callbacks.connectionsAccepted(), 25);
  EXPECT_GE(server_callbacks.requestsReceived(), 10);
  // Server does not close its own sockets but instead relies on the client to initate the close
  EXPECT_EQ(0, server_callbacks.localCloses());
  // Server sees a client-initiated close for every socket it accepts
  EXPECT_EQ(server_callbacks.remoteCloses(), server_callbacks.connectionsAccepted());
}

TEST_P(ClientServerTest, AutoConcurrencyRun) {
  std::vector<const char*> argv;
  argv.push_back("foo");
  argv.push_back("--concurrency");
  argv.push_back("auto");
  argv.push_back("--address-family");
  argv.push_back(getAddressFamilyOptionString());
  argv.push_back("--duration");
  argv.push_back("2");
  argv.push_back("--rps");
  argv.push_back("5");
  argv.push_back("--verbosity");
  argv.push_back("error");
  std::string url = testUrl();
  argv.push_back(url.c_str());
  Mixer::Integration::ServerCallbackHelper server_callbacks; // sends a 200 OK to everything
  Main program(argv.size(), argv.data());

  server_.start(server_callbacks);
  EXPECT_TRUE(program.run());
  // each worker ought to have created a single connection.
  EXPECT_EQ(server_callbacks.connectionsAccepted(), PlatformUtils::determineCpuCoresWithAffinity());
  EXPECT_GE(server_callbacks.requestsReceived(), 1);
  // Server does not close its own sockets but instead relies on the client to initate the close
  EXPECT_EQ(0, server_callbacks.localCloses());
  // Server sees a client-initiated close for every socket it accepts
  EXPECT_EQ(server_callbacks.remoteCloses(), server_callbacks.connectionsAccepted());
}

TEST_P(ClientServerTest, BadRun) {
  Main program(Nighthawk::Client::TestUtility::createOptionsImpl(
      fmt::format("foo --address-family {} --duration 1 --rps 1 https://unresolveable.host/",
                  getAddressFamilyOptionString())));
  EXPECT_FALSE(program.run());
}

} // namespace Client
} // namespace Nighthawk
