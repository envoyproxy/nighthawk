#include "nighthawk/common/exception.h"

#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/utility.h"

#include "api/client/service.pb.h"

#include "client/output_transform_main.h"

#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {
namespace Client {

class OutputTransformMainTest : public Test {
public:
  std::stringstream stream_;
};

TEST_F(OutputTransformMainTest, BadArgs) {
  std::vector<const char*> argv = {"foo", "bar"};
  stream_ << "foo bar blah";
  EXPECT_THROW(OutputTransformMain(argv.size(), argv.data(), stream_), std::exception);
}

TEST_F(OutputTransformMainTest, BadOutputFormat) {
  std::vector<const char*> argv = {"foo", "--output-format", "nonsense"};
  EXPECT_THROW(OutputTransformMain(argv.size(), argv.data(), stream_), std::exception);
}

// Correct args, but empty stdin input
TEST_F(OutputTransformMainTest, NoInput) {
  std::vector<const char*> argv = {"foo", "--output-format", "human"};
  OutputTransformMain main(argv.size(), argv.data(), stream_);
  EXPECT_NE(main.run(), 0);
}

// Correct args, but fails to parse as json input
TEST_F(OutputTransformMainTest, BadInput) {
  std::vector<const char*> argv = {"foo", "--output-format", "human"};
  stream_ << "foo bar blah";
  OutputTransformMain main(argv.size(), argv.data(), stream_);
  EXPECT_NE(main.run(), 0);
}

// Correct args, correct json, but doesn't validate (misses URI).
TEST_F(OutputTransformMainTest, JsonNotValidating) {
  std::vector<const char*> argv = {"foo", "--output-format", "human"};
  stream_ << "{invalid_field:1}";
  OutputTransformMain main(argv.size(), argv.data(), stream_);
  EXPECT_NE(main.run(), 0);
}

TEST_F(OutputTransformMainTest, HappyFlow) {
  std::vector<const char*> argv = {"foo", "--output-format", "human"};
  nighthawk::client::Output output;
  output.mutable_options()->mutable_uri()->set_value("http://127.0.0.1/");
  stream_ << Envoy::MessageUtil::getJsonStringFromMessage(output, true, true);
  OutputTransformMain main(argv.size(), argv.data(), stream_);
  EXPECT_EQ(main.run(), 0);
}

} // namespace Client
} // namespace Nighthawk
