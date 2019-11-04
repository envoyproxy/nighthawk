#include <chrono>

#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/utility.h"

#include "client/client.h"

#include "test/client/utility.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

class ClientTest : public testing::Test {};

// TODO(https://github.com/envoyproxy/nighthawk/issues/179): revisit this, and improve testability
// of the Main class, so we can mock its dependencies. We now have integration tests covering this
// much better.

// Note: these tests do not have a backend set up to talk to.
// That's why we expect exit codes indicating failure.
TEST_F(ClientTest, NormalRun) {
  Main program(Nighthawk::Client::TestUtility::createOptionsImpl(
      "foo --duration 1 --rps 10 http://localhost:63657/"));
  EXPECT_FALSE(program.run());
}

TEST_F(ClientTest, AutoConcurrencyRun) {
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
  argv.push_back("http://localhost:63657/");
  Main program(argv.size(), argv.data());
  EXPECT_FALSE(program.run());
}

// TODO(https://github.com/envoyproxy/nighthawk/issues/140):
// This is just for coverage, and we do not care where any traffic we send ends it or what that
// looks like. We do functional testing in python now, but unfortunately any code we hit there isn't
// counted as code-coverage. Ideally, the code hit during the python test runs would count for
// coverage, and we use unit-tests here to hit any edge cases we can't easily hit otherwise.
TEST_F(ClientTest, TracingRun) {
  std::vector<const char*> argv;
  argv.push_back("foo");
  argv.push_back("--duration");
  argv.push_back("5");
  argv.push_back("--rps");
  argv.push_back("10");
  argv.push_back("--verbosity");
  argv.push_back("error");
  argv.push_back("http://localhost:63657/");
  argv.push_back("--trace");
  argv.push_back("zipkin://localhost:9411/api/v1/spans");
  Main program(argv.size(), argv.data());
  EXPECT_FALSE(program.run());
}

TEST_F(ClientTest, BadRun) {
  Main program(Nighthawk::Client::TestUtility::createOptionsImpl(
      "foo --duration 1 --rps 1 https://unresolveable.host/"));
  EXPECT_FALSE(program.run());
}

} // namespace Client
} // namespace Nighthawk
