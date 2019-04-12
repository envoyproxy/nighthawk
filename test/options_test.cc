#include "gtest/gtest.h"

#include "test/test_common/utility.h"

#include "nighthawk/source/client/options_impl.h"

using namespace std::chrono_literals;

namespace Nighthawk {
namespace Client {

class OptionsImplTest : public testing::Test {
public:
  OptionsImplTest()
      : client_name_("nighthawk_client"), good_test_uri_("http://127.0.0.1/"),
        no_arg_match_("Couldn't find match for argument") {}

  std::unique_ptr<OptionsImpl> createOptionsImpl(const std::string& args) {
    std::vector<std::string> words = Envoy::TestUtility::split(args, ' ');
    std::vector<const char*> argv;
    for (const std::string& s : words) {
      argv.push_back(s.c_str());
    }
    return std::make_unique<OptionsImpl>(argv.size(), argv.data());
  }

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
  EXPECT_THROW_WITH_REGEX(createOptionsImpl(fmt::format("{} --foo", client_name_)),
                          MalformedArgvException, "Invalid URI");
}

TEST_F(OptionsImplTest, All) {
  std::unique_ptr<OptionsImpl> options =
      createOptionsImpl(fmt::format("{} --rps 4 --connections 5 --duration 6 --timeout 7 --h2 "
                                    "--concurrency 8 --verbosity error --output-format json {}",
                                    client_name_, good_test_uri_));

  EXPECT_EQ(4, options->requestsPerSecond());
  EXPECT_EQ(5, options->connections());
  EXPECT_EQ(6s, options->duration());
  EXPECT_EQ(7s, options->timeout());
  EXPECT_EQ(true, options->h2());
  EXPECT_EQ("8", options->concurrency());
  EXPECT_EQ("error", options->verbosity());
  EXPECT_EQ("json", options->outputFormat());
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
  EXPECT_EQ(cmd->uri(), options->uri());
}

TEST_F(OptionsImplTest, Help) {
  EXPECT_THROW_WITH_REGEX(createOptionsImpl(fmt::format("{}  --help", client_name_)),
                          NoServingException, "NoServingException");
}

TEST_F(OptionsImplTest, Version) {
  EXPECT_THROW_WITH_REGEX(createOptionsImpl(fmt::format("{}  --version", client_name_)),
                          NoServingException, "NoServingException");
}

TEST_F(OptionsImplTest, NoArguments) {
  EXPECT_THROW_WITH_REGEX(createOptionsImpl(fmt::format("{}", client_name_)),
                          MalformedArgvException, "Required argument missing: uri");
}

TEST_P(OptionsImplIntTest, IntOptionsBadValuesThrow) {
  const char* option_name = GetParam();
  EXPECT_THROW_WITH_REGEX(
      createOptionsImpl(fmt::format("{} {} --{} 0", client_name_, good_test_uri_, option_name)),
      MalformedArgvException, fmt::format("Invalid value for --{}", option_name));
  EXPECT_THROW_WITH_REGEX(
      createOptionsImpl(fmt::format("{} {} --{} -1", client_name_, good_test_uri_, option_name)),
      MalformedArgvException, fmt::format("Invalid value for --{}", option_name));
  EXPECT_THROW_WITH_REGEX(
      createOptionsImpl(fmt::format("{} {} --{}", client_name_, good_test_uri_, option_name)),
      MalformedArgvException, "Missing a value for this argument");
  EXPECT_THROW_WITH_REGEX(
      createOptionsImpl(fmt::format("{} {} --{} foo", client_name_, good_test_uri_, option_name)),
      MalformedArgvException, "Couldn't read argument value");
}

INSTANTIATE_TEST_SUITE_P(IntOptionTests, OptionsImplIntTest,
                         testing::Values("rps", "connections", "duration", "timeout"));

TEST_F(OptionsImplTest, BadH2FlagThrows) {
  EXPECT_THROW_WITH_REGEX(
      createOptionsImpl(fmt::format("{} {} --h2 0", client_name_, good_test_uri_)),
      MalformedArgvException, "Couldn't find match for argument");
  EXPECT_THROW_WITH_REGEX(
      createOptionsImpl(fmt::format("{} {} --h2 true", client_name_, good_test_uri_)),
      MalformedArgvException, "Couldn't find match for argument");
}

TEST_F(OptionsImplTest, BadConcurrencyValuesThrow) {
  EXPECT_THROW_WITH_REGEX(
      createOptionsImpl(fmt::format("{} {} --concurrency 0", client_name_, good_test_uri_)),
      MalformedArgvException, "Value for --concurrency should be greater then 0.");
  EXPECT_THROW_WITH_REGEX(
      createOptionsImpl(fmt::format("{} {} --concurrency -1", client_name_, good_test_uri_)),
      MalformedArgvException, "Value for --concurrency should be greater then 0.");
  EXPECT_THROW_WITH_REGEX(
      createOptionsImpl(fmt::format("{} {} --concurrency", client_name_, good_test_uri_)),
      MalformedArgvException, "Missing a value for this argument");
  EXPECT_THROW_WITH_REGEX(
      createOptionsImpl(fmt::format("{} {} --concurrency foo", client_name_, good_test_uri_)),
      MalformedArgvException, "Invalid value for --concurrency");
  EXPECT_THROW_WITH_REGEX(createOptionsImpl(fmt::format("{} {} --concurrency 999999999999999999999",
                                                        client_name_, good_test_uri_)),
                          MalformedArgvException, "Value out of range: --concurrency");
}

TEST_F(OptionsImplTest, AutoConcurrencyValueParsedOK) {
  std::unique_ptr<OptionsImpl> options =
      createOptionsImpl(fmt::format("{} --concurrency auto {} ", client_name_, good_test_uri_));
  EXPECT_EQ("auto", options->concurrency());
}

TEST_F(OptionsImplTest, VerbosityValuesAreConstrained) {
  EXPECT_THROW_WITH_REGEX(
      createOptionsImpl(fmt::format("{} {} --verbosity foo", client_name_, good_test_uri_)),
      MalformedArgvException, "Value 'foo' does not meet constraint");
}

// TODO(oschaaf): URI parsing/validation is weaker then it should be at the moment.
TEST_F(OptionsImplTest, InacceptibleUri) {
  EXPECT_THROW_WITH_REGEX(createOptionsImpl(fmt::format("{} bad://127.0.0.1/", client_name_)),
                          MalformedArgvException, "Invalid URI");
}

} // namespace Client
} // namespace Nighthawk
