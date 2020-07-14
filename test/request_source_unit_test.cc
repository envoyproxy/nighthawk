#include <chrono>

#include "external/envoy/test/test_common/utility.h"

#include "common/request_source_impl.h"

#include "test/mocks/common/mock_request_source.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {

class MockRPCRequestSource : public MockRequestSource {};

class RequestSourceUnitTest : public testing::Test {
protected:
  // You can remove any or all of the following functions if their bodies would
  // be empty.
  //  RequestSource* request_generator_;
  const RequestGenerator request_generator_;
  RequestSourceUnitTest() {
    // You can do set-up work for each test here.
    request_generator_ = new MockRequestSource();
  }

  ~RequestSourceUnitTest() override {
    // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

  // Class members declared here can be used by all tests in the test suite
  // for Foo.
};
TEST_F(RequestSourceUnitTest, MockRPCRequestSource) {
  auto request = request_generator_();
  EXPECT_TRUE(AssertionFailure() << "Not Implemented");
}

} // namespace Nighthawk
