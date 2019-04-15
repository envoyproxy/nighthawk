#include "gtest/gtest.h"

#include "external/envoy/test/test_common/utility.h"
#include "test/client/utility.h"

#include "client/options_impl.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

class OptionsImplTest : public testing::Test {
public:
  OptionsImplTest()
      : client_name_("nighthawk_client"), good_test_uri_("http://127.0.0.1/"),
        no_arg_match_("Couldn't find match for argument") {}

  std::string client_name_;
  std::string good_test_uri_;
  std::string no_arg_match_;
};

class OptionsImplIntTest : public OptionsImplTest,
                           public testing::WithParamInterface<const char*> {};

TEST_F(OptionsImplTest, BogusInput) {
  // When just passing the non-existing argument --foo it would be interpreted as a
  // hostname. However, hostnames shouldn't start with '-', and hence this test should
  // not pass.
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} --foo", client_name_)),
                          MalformedArgvException, "Invalid URI");
}

TEST_F(OptionsImplTest, All) {
  std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(fmt::format(
      "{} --rps 4 --connections 5 --duration 6 --timeout 7 --h2 "
      "--concurrency 8 --verbosity error --output-format json --prefetch-connections {}",
      client_name_, good_test_uri_));

  EXPECT_EQ(4, options->requestsPerSecond());
  EXPECT_EQ(5, options->connections());
  EXPECT_EQ(6s, options->duration());
  EXPECT_EQ(7s, options->timeout());
  EXPECT_EQ(true, options->h2());
  EXPECT_EQ("8", options->concurrency());
  EXPECT_EQ("error", options->verbosity());
  EXPECT_EQ("json", options->outputFormat());
  EXPECT_EQ(true, options->prefetchConnections());
  EXPECT_EQ(good_test_uri_, options->uri());

  // Check that our conversion to CommandLineOptionsPtr makes sense.
  CommandLineOptionsPtr cmd = options->toCommandLineOptions();
  EXPECT_EQ(cmd->requests_per_second(), options->requestsPerSecond());
  EXPECT_EQ(cmd->connections(), options->connections());
  EXPECT_EQ(cmd->duration().seconds(), options->duration().count());
  EXPECT_EQ(cmd->timeout().seconds(), options->timeout().count());
  EXPECT_EQ(cmd->h2(), options->h2());
  EXPECT_EQ(cmd->concurrency(), options->concurrency());
  EXPECT_EQ(cmd->verbosity(), options->verbosity());
  EXPECT_EQ(cmd->output_format(), options->outputFormat());
  EXPECT_EQ(cmd->prefetch_connections(), options->prefetchConnections());
  EXPECT_EQ(cmd->uri(), options->uri());
}

TEST_F(OptionsImplTest, Help) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{}  --help", client_name_)),
                          NoServingException, "NoServingException");
}

TEST_F(OptionsImplTest, Version) {
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{}  --version", client_name_)),
      NoServingException, "NoServingException");
}

TEST_F(OptionsImplTest, NoArguments) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{}", client_name_)),
                          MalformedArgvException, "Required argument missing: uri");
}

TEST_P(OptionsImplIntTest, IntOptionsBadValuesThrow) {
  const char* option_name = GetParam();
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} {} --{} 0", client_name_,
                                                                     good_test_uri_, option_name)),
                          MalformedArgvException,
                          fmt::format("Invalid value for --{}", option_name));
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} {} --{} -1", client_name_,
                                                                     good_test_uri_, option_name)),
                          MalformedArgvException,
                          fmt::format("Invalid value for --{}", option_name));
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} {} --{}", client_name_, good_test_uri_, option_name)),
                          MalformedArgvException, "Missing a value for this argument");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} {} --{} foo", client_name_,
                                                                     good_test_uri_, option_name)),
                          MalformedArgvException, "Couldn't read argument value");
}

INSTANTIATE_TEST_SUITE_P(IntOptionTests, OptionsImplIntTest,
                         testing::Values("rps", "connections", "duration", "timeout"));

TEST_F(OptionsImplTest, BadH2FlagThrows) {
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} {} --h2 0", client_name_, good_test_uri_)),
      MalformedArgvException, "Couldn't find match for argument");
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} {} --h2 true", client_name_, good_test_uri_)),
      MalformedArgvException, "Couldn't find match for argument");
}

TEST_F(OptionsImplTest, H2Flag) {
  EXPECT_FALSE(
      TestUtility::createOptionsImpl(fmt::format("{} {}", client_name_, good_test_uri_))->h2());
  EXPECT_TRUE(
      TestUtility::createOptionsImpl(fmt::format("{} {} --h2", client_name_, good_test_uri_))
          ->h2());
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} {} --h2 0", client_name_, good_test_uri_)),
      MalformedArgvException, "Couldn't find match for argument");
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} {} --h2 true", client_name_, good_test_uri_)),
      MalformedArgvException, "Couldn't find match for argument");
}

TEST_F(OptionsImplTest, PrefetchConnectionsFlag) {
  EXPECT_FALSE(TestUtility::createOptionsImpl(fmt::format("{} {}", client_name_, good_test_uri_))
                   ->prefetchConnections());
  EXPECT_TRUE(TestUtility::createOptionsImpl(
                  fmt::format("{} {} --prefetch-connections", client_name_, good_test_uri_))
                  ->prefetchConnections());
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} {} --prefetch-connections 0", client_name_, good_test_uri_)),
                          MalformedArgvException, "Couldn't find match for argument");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} {} --prefetch-connections true", client_name_, good_test_uri_)),
                          MalformedArgvException, "Couldn't find match for argument");
}

TEST_F(OptionsImplTest, BadConcurrencyValuesThrow) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} {} --concurrency 0", client_name_, good_test_uri_)),
                          MalformedArgvException,
                          "Value for --concurrency should be greater then 0.");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} {} --concurrency -1", client_name_, good_test_uri_)),
                          MalformedArgvException,
                          "Value for --concurrency should be greater then 0.");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} {} --concurrency", client_name_, good_test_uri_)),
                          MalformedArgvException, "Missing a value for this argument");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} {} --concurrency foo", client_name_, good_test_uri_)),
                          MalformedArgvException, "Invalid value for --concurrency");
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(
          fmt::format("{} {} --concurrency 999999999999999999999", client_name_, good_test_uri_)),
      MalformedArgvException, "Value out of range: --concurrency");
}

TEST_F(OptionsImplTest, AutoConcurrencyValueParsedOK) {
  std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(
      fmt::format("{} --concurrency auto {} ", client_name_, good_test_uri_));
  EXPECT_EQ("auto", options->concurrency());
}

TEST_F(OptionsImplTest, VerbosityValuesAreConstrained) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} {} --verbosity foo", client_name_, good_test_uri_)),
                          MalformedArgvException, "Value 'foo' does not meet constraint");
}

// TODO(oschaaf): URI parsing/validation is weaker then it should be at the moment.
TEST_F(OptionsImplTest, InacceptibleUri) {
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} bad://127.0.0.1/", client_name_)),
      MalformedArgvException, "Invalid URI");
}

} // namespace Client
} // namespace Nighthawk
