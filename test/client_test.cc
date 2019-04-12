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

#include "nighthawk/test/mocks.h"

#include "nighthawk/source/client/client.h"
#include "nighthawk/source/client/factories_impl.h"
#include "nighthawk/source/client/options_impl.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

class ClientTest : public Envoy::BaseIntegrationTest,
                   public testing::TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  ClientTest() : Envoy::BaseIntegrationTest(GetParam(), realTime(), readEnvoyConfiguration()) {}

  std::string readEnvoyConfiguration() {
    Envoy::Filesystem::InstanceImplPosix file_system;
    const std::string config = file_system.fileReadToEnd(Envoy::TestEnvironment::runfilesPath(
        "nighthawk/test/test_data/benchmark_http_client_test_envoy.yaml"));
    return Envoy::TestEnvironment::substitute(config);
  }

  void SetUp() override {
    // We fork the integration test fixture into a child process, to avoid conflicting
    // runtimeloaders as both NH and the integration server want to own that and we can have only
    // one. The plan is to move to python for this type of testing, so hopefully we can deprecate
    // this test and it's peculiar setup with fork/pipe soon.
    RELEASE_ASSERT(pipe(fd_port_) == 0, "Failed to open pipe");
    RELEASE_ASSERT(pipe(fd_confirm_) == 0, "Failed to open pipe");
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

  std::unique_ptr<OptionsImpl> createOptionsImpl(const std::string& args) {
    std::vector<std::string> words = Envoy::TestUtility::split(args, ' ');
    std::vector<const char*> argv;
    for (const std::string& s : words) {
      argv.push_back(s.c_str());
    }
    return std::make_unique<OptionsImpl>(argv.size(), argv.data());
  }

  int port_;
  pid_t pid_;
  int fd_port_[2];
  int fd_confirm_[2];
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ClientTest,
                         testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(ClientTest, NormalRun) {
  Main program(createOptionsImpl(fmt::format("foo --duration 2 --rps 10 {}", testUrl())));
  EXPECT_TRUE(program.run());
}

TEST_P(ClientTest, AutoConcurrencyRun) {
  std::vector<const char*> argv;
  argv.push_back("foo");
  argv.push_back("--concurrency");
  argv.push_back("auto");
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
  Main program(createOptionsImpl("foo --duration 1 --rps 1 https://unresolveable.host/"));
  EXPECT_FALSE(program.run());
}

} // namespace Client
} // namespace Nighthawk
