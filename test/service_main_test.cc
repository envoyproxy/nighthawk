#include <grpc++/grpc++.h>

#include <chrono>
#include <csignal>
#include <thread>

#include "nighthawk/common/exception.h"

#include "client/service_main.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

class ServiceMainTest : public Test {};

// TODO(oschaaf): this gets us some coverage, but we need more functional testing.
// See if we can add some python integration tests.
TEST_F(ServiceMainTest, HelloWorld) {
  std::vector<const char*> argv;
  argv.push_back("foo");
  ServiceMain service(argv.size(), argv.data());
  service.Start();
  service.Shutdown();
}

TEST_F(ServiceMainTest, BadArgs) {
  std::vector<const char*> argv;
  argv.push_back("foo");
  argv.push_back("bar");

  EXPECT_THROW(ServiceMain(argv.size(), argv.data()), std::exception);
}

TEST_F(ServiceMainTest, BadIpAddress) {
  std::vector<const char*> argv;
  argv.push_back("foo");
  argv.push_back("--listen");
  argv.push_back("bar");

  EXPECT_THROW(ServiceMain(argv.size(), argv.data()), NighthawkException);
}

TEST_F(ServiceMainTest, Unbindable) {
  std::vector<const char*> argv;
  argv.push_back("foo");
  argv.push_back("--listen");
  argv.push_back("255.255.255.255:10");
  ServiceMain service_main(argv.size(), argv.data());
  EXPECT_THROW(service_main.Start(), NighthawkException);
}

} // namespace Client
} // namespace Nighthawk
