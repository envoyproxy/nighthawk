#include <grpc++/grpc++.h>

#include <chrono>
#include <csignal>
#include <thread>

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
  std::thread t1([&service] { service.Run(); });
  sleep(1);
  service.Shutdown();
  t1.join();
}

TEST_F(ServiceMainTest, BadArgs) {
  std::vector<const char*> argv;
  argv.push_back("foo");
  argv.push_back("bar");

  EXPECT_THROW(ServiceMain(argv.size(), argv.data()), std::exception);
}

} // namespace Client
} // namespace Nighthawk
