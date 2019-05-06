// This test relies on fork(). Somehow the integration test server excepts when running
// under TSAN because of Envoy not being able to find its configuration file with die_after_fork
// disabled. The code below seems to suggest that TSAN doesn't support this scenario.
// https://github.com/llvm-mirror/compiler-rt/blob/master/lib/tsan/rtl/tsan_interceptors.cc#L999
// Further root cause analysis is not worth it at this point, because this code will be deprecated
// soon in favor of either https://github.com/envoyproxy/nighthawk/pull/60 or moving this end-to-end
// testing to python.
// Thus we just disable this test when we detect we are running under TSAN.
#ifndef __has_feature
#define __has_feature(x) 0 // Compatibility with non-clang compilers.
#endif
#if defined(__has_feature) && !__has_feature(thread_sanitizer)

#include <chrono>

#include "common/api/api_impl.h"
#include "common/common/thread_impl.h"
#include "common/filesystem/filesystem_impl.h"

#include "client/client.h"
#include "client/factories_impl.h"
#include "client/options_impl.h"

#include "test/client/utility.h"
#include "test/integration/http_integration.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"
#include "test/mocks.h"
#include "test/mocks/event/mocks.h"
#include "test/mocks/stats/mocks.h"
#include "test/server/utility.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

class ClientTest : public Envoy::BaseIntegrationTest,
                   public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  ClientTest()
      : Envoy::BaseIntegrationTest(GetParam(), realTime(), readEnvoyConfiguration()),
        fd_port_(2, 0), fd_confirm_(2, 0) {}

  std::string readEnvoyConfiguration() {
    Envoy::Filesystem::InstanceImplPosix file_system;
    std::string envoy_config = file_system.fileReadToEnd(Envoy::TestEnvironment::runfilesPath(
        "test/test_data/benchmark_http_client_test_envoy.yaml"));
    return Envoy::TestEnvironment::substitute(envoy_config);
  }

  void SetUp() override {
    // We fork the integration test fixture into a child process, to avoid conflicting
    // runtimeloaders as both NH and the integration server want to own that and we can have only
    // one. The plan is to move to python for this type of testing, so hopefully we can deprecate
    // this test and it's peculiar setup with fork/pipe soon.
    RELEASE_ASSERT(pipe(fd_port_.data()) == 0, "Failed to open pipe");
    RELEASE_ASSERT(pipe(fd_confirm_.data()) == 0, "Failed to open pipe");
    pid_ = fork();
    RELEASE_ASSERT(pid_ >= 0, "Fork failed");

    if (pid_ == 0) {
      // child process running the integration test server.
      ares_library_init(ARES_LIB_INIT_ALL);
      Envoy::Event::Libevent::Global::initialize();
      initialize();
      int port = lookupPort("listener_0");
      int parent_message;
      RELEASE_ASSERT(write(fd_port_[1], &port, sizeof(port)) == sizeof(port), "write failed");
      // The parent process writes to fd_confirm_ when it has read the port. This call to read
      // blocks until that happens.
      RELEASE_ASSERT(read(fd_confirm_[0], &parent_message, sizeof(parent_message)) ==
                         sizeof(parent_message),
                     "Invalid read size");
      RELEASE_ASSERT(parent_message == port, "Failed to confirm port");
      // The parent process closes fd_port_ when the test tears down. The read call blocks until it
      // does that.
      RELEASE_ASSERT(read(fd_port_[0], &port_, sizeof(port_)) == -1, "read failed");
      GTEST_SKIP();
    } else if (pid_ > 0) {
      RELEASE_ASSERT(read(fd_port_[0], &port_, sizeof(port_)) > 0, "read failed");
      RELEASE_ASSERT(port_ > 0, "read unexpected port_ value");
      RELEASE_ASSERT(write(fd_confirm_[1], &port_, sizeof(port_)) == sizeof(port_), "write failed");
    }
  }

  void TearDown() override {
    if (pid_ == 0) {
      test_server_.reset();
      fake_upstreams_.clear();
      ares_library_cleanup();
    }
    RELEASE_ASSERT(!close(fd_confirm_[0]), "close failed");
    RELEASE_ASSERT(!close(fd_confirm_[1]), "close failed");
    RELEASE_ASSERT(!close(fd_port_[0]), "close failed");
    RELEASE_ASSERT(!close(fd_port_[1]), "close failed");
  }

  std::string testUrl() {
    RELEASE_ASSERT(pid_ > 0, "Unexpected call to testUrl");
    const std::string address = Envoy::Network::Test::getLoopbackAddressUrlString(GetParam());
    return fmt::format("http://{}:{}/", address, port_);
  }

  const char* getAddressFamilyOptionString() {
    auto ip_version = GetParam();
    RELEASE_ASSERT(ip_version == Envoy::Network::Address::IpVersion::v4 ||
                       ip_version == Envoy::Network::Address::IpVersion::v6,
                   "bad ip version");
    return ip_version == Envoy::Network::Address::IpVersion::v6 ? "v6" : "v4";
  }

  int port_;
  pid_t pid_;
  std::vector<int> fd_port_;
  std::vector<int> fd_confirm_;
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ClientTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(ClientTest, NormalRun) {
  Main program(Nighthawk::Client::TestUtility::createOptionsImpl(
      fmt::format("foo --address-family {} --duration 2 --rps 10 {}",
                  getAddressFamilyOptionString(), testUrl())));
  EXPECT_TRUE(program.run());
}

TEST_P(ClientTest, AutoConcurrencyRun) {
  std::vector<const char*> argv;
  argv.push_back("foo");
  argv.push_back("--concurrency");
  argv.push_back("auto");
  argv.push_back("--address-family");
  argv.push_back(getAddressFamilyOptionString());
  argv.push_back("--duration");
  argv.push_back("1");
  argv.push_back("--rps");
  argv.push_back("1");
  argv.push_back("--verbosity");
  argv.push_back("error");
  std::string url = testUrl();
  argv.push_back(url.c_str());

  Main program(argv.size(), argv.data());
  EXPECT_TRUE(program.run());
}

TEST_P(ClientTest, BadRun) {
  Main program(Nighthawk::Client::TestUtility::createOptionsImpl(
      fmt::format("foo --address-family {} --duration 1 --rps 1 https://unresolveable.host/",
                  getAddressFamilyOptionString())));
  EXPECT_FALSE(program.run());
}

} // namespace Client
} // namespace Nighthawk

#endif