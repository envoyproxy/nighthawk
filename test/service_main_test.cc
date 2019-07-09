#include <grpc++/grpc++.h>

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
  std::vector<const char*> argv = {"foo"};
  ServiceMain service(argv.size(), argv.data());
  service.start();
  service.shutdown();
}

TEST_F(ServiceMainTest, BadArgs) {
  std::vector<const char*> argv = {"foo", "bar"};
  EXPECT_THROW(ServiceMain(argv.size(), argv.data()), std::exception);
}

TEST_F(ServiceMainTest, BadIpAddress) {
  std::vector<const char*> argv = {"foo", "--listen", "bar"};
  EXPECT_THROW(ServiceMain(argv.size(), argv.data()), NighthawkException);
}

TEST_F(ServiceMainTest, Unbindable) {
  std::vector<const char*> argv = {"foo", "--listen", "1.1.1.1:10"};
  ServiceMain service_main(argv.size(), argv.data());
  EXPECT_THROW(service_main.start(), NighthawkException);
}

TEST_F(ServiceMainTest, PortZero) {
  std::vector<const char*> argv = {"foo", "--listen", "127.0.0.1:0"};
  ServiceMain service_main(argv.size(), argv.data());
  EXPECT_NO_THROW(service_main.start());
}

} // namespace Client
} // namespace Nighthawk
