#include "external/envoy/test/test_common/utility.h"

#include "client/options_impl.h"

#include "test/client/utility.h"

#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {
namespace Client {

class OptionsImplTest : public Test {
public:
  OptionsImplTest()
      : client_name_("nighthawk_client"), good_test_uri_("http://127.0.0.1/"),
        no_arg_match_("Couldn't find match for argument") {}

  std::string client_name_;
  std::string good_test_uri_;
  std::string no_arg_match_;
};

class OptionsImplIntTest : public OptionsImplTest, public WithParamInterface<const char*> {};
class OptionsImplIntTestNonZeroable : public OptionsImplTest,
                                      public WithParamInterface<const char*> {};

TEST_F(OptionsImplTest, BogusInput) {
  // When just passing the non-existing argument --foo it would be interpreted as a
  // hostname. However, hostnames shouldn't start with '-', and hence this test should
  // not pass.
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} --foo", client_name_)),
                          MalformedArgvException, "Invalid URI");
}

// This test should cover every option we offer.
TEST_F(OptionsImplTest, All) {
  Envoy::MessageUtil util;
  std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(fmt::format(
      "{} --rps 4 --connections 5 --duration 6 --timeout 7 --h2 "
      "--concurrency 8 --verbosity error --output-format yaml --prefetch-connections "
      "--burst-size 13 --address-family v6 --request-method POST --request-body-size 1234 "
      "--tls-context {} --request-header f1:b1 --request-header f2:b2 {} --max-pending-requests 10 "
      "--max-active-requests 11 --max-requests-per-connection 12 --sequencer-idle-strategy sleep "
      "--termination-predicate t1:1 --termination-predicate t2:2 --failure-predicate f1:1 "
      "--failure-predicate f2:2 ",
      client_name_,
      "{common_tls_context:{tls_params:{cipher_suites:[\"-ALL:ECDHE-RSA-AES256-GCM-SHA384\"]}}}",
      good_test_uri_));

  EXPECT_EQ(4, options->requestsPerSecond());
  EXPECT_EQ(5, options->connections());
  EXPECT_EQ(6s, options->duration());
  EXPECT_EQ(7s, options->timeout());
  EXPECT_EQ(true, options->h2());
  EXPECT_EQ("8", options->concurrency());
  EXPECT_EQ(nighthawk::client::Verbosity::ERROR, options->verbosity());
  EXPECT_EQ(nighthawk::client::OutputFormat::YAML, options->outputFormat());
  EXPECT_EQ(true, options->prefetchConnections());
  EXPECT_EQ(13, options->burstSize());
  EXPECT_EQ(nighthawk::client::AddressFamily::V6, options->addressFamily());
  EXPECT_EQ(good_test_uri_, options->uri());
  EXPECT_EQ(envoy::api::v2::core::RequestMethod::POST, options->requestMethod());
  const std::vector<std::string> expected_headers = {"f1:b1", "f2:b2"};
  EXPECT_EQ(expected_headers, options->requestHeaders());
  EXPECT_EQ(1234, options->requestBodySize());
  EXPECT_EQ("common_tls_context {\n  tls_params {\n    cipher_suites: "
            "\"-ALL:ECDHE-RSA-AES256-GCM-SHA384\"\n  }\n}\n",
            options->tlsContext().DebugString());
  EXPECT_EQ(10, options->maxPendingRequests());
  EXPECT_EQ(11, options->maxActiveRequests());
  EXPECT_EQ(12, options->maxRequestsPerConnection());
  EXPECT_EQ(nighthawk::client::SequencerIdleStrategy::SLEEP, options->sequencerIdleStrategy());
  ASSERT_EQ(2, options->terminationPredicates().size());
  EXPECT_EQ(1, options->terminationPredicates()["t1"]);
  EXPECT_EQ(2, options->terminationPredicates()["t2"]);
  ASSERT_EQ(2, options->failurePredicates().size());
  EXPECT_EQ(1, options->failurePredicates()["f1"]);
  EXPECT_EQ(2, options->failurePredicates()["f2"]);

  // Check that our conversion to CommandLineOptionsPtr makes sense.
  CommandLineOptionsPtr cmd = options->toCommandLineOptions();
  EXPECT_EQ(cmd->requests_per_second().value(), options->requestsPerSecond());
  EXPECT_EQ(cmd->connections().value(), options->connections());
  EXPECT_EQ(cmd->duration().seconds(), options->duration().count());
  EXPECT_EQ(cmd->timeout().seconds(), options->timeout().count());
  EXPECT_EQ(cmd->h2().value(), options->h2());
  EXPECT_EQ(cmd->concurrency().value(), options->concurrency());
  EXPECT_EQ(cmd->verbosity().value(), options->verbosity());
  EXPECT_EQ(cmd->output_format().value(), options->outputFormat());
  EXPECT_EQ(cmd->prefetch_connections().value(), options->prefetchConnections());
  EXPECT_EQ(cmd->burst_size().value(), options->burstSize());
  EXPECT_EQ(cmd->address_family().value(), options->addressFamily());
  EXPECT_EQ(cmd->uri().value(), options->uri());
  EXPECT_EQ(cmd->request_options().request_method(), cmd->request_options().request_method());
  EXPECT_EQ(expected_headers.size(), cmd->request_options().request_headers_size());

  int i = 0;
  for (const auto& header : cmd->request_options().request_headers()) {
    EXPECT_EQ(expected_headers[i++], header.header().key() + ":" + header.header().value());
  }

  EXPECT_EQ(cmd->request_options().request_body_size().value(), options->requestBodySize());
  EXPECT_TRUE(util(cmd->tls_context(), options->tlsContext()));
  EXPECT_EQ(cmd->max_pending_requests().value(), options->maxPendingRequests());
  EXPECT_EQ(cmd->max_active_requests().value(), options->maxActiveRequests());
  EXPECT_EQ(cmd->max_requests_per_connection().value(), options->maxRequestsPerConnection());
  EXPECT_EQ(cmd->sequencer_idle_strategy().value(), options->sequencerIdleStrategy());

  ASSERT_EQ(2, cmd->termination_predicates_size());
  EXPECT_EQ(cmd->termination_predicates().at("t1"), 1);
  EXPECT_EQ(cmd->termination_predicates().at("t2"), 2);
  ASSERT_EQ(2, cmd->failure_predicates_size());
  EXPECT_EQ(cmd->failure_predicates().at("f1"), 1);
  EXPECT_EQ(cmd->failure_predicates().at("f2"), 2);

  // Now we construct a new options from the proto we created above. This should result in an
  // OptionsImpl instance equivalent to options. We test that by converting both to yaml strings,
  // expecting them to be equal. This should provide helpful output when the test fails by showing
  // the unexpected (yaml) diff.
  // The predicates are defined as proto maps, and these seem to re-serialize into a different
  // order. Hence we trim the maps to contain a single entry so they don't thwart our textual
  // comparison below.
  EXPECT_EQ(1, cmd->mutable_failure_predicates()->erase("f1"));
  EXPECT_EQ(1, cmd->mutable_termination_predicates()->erase("t1"));

  OptionsImpl options_from_proto(*cmd);
  std::string s1 = Envoy::MessageUtil::getYamlStringFromMessage(
      *(options_from_proto.toCommandLineOptions()), true, true);
  std::string s2 = Envoy::MessageUtil::getYamlStringFromMessage(*cmd, true, true);

  EXPECT_EQ(s1, s2);
  // For good measure, also directly test for proto equivalence, though this should be
  // superfluous.
  EXPECT_TRUE(util(*(options_from_proto.toCommandLineOptions()), *cmd));
}

// Test that TCLAP's way of handling --help behaves as expected.
TEST_F(OptionsImplTest, Help) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{}  --help", client_name_)),
                          NoServingException, "NoServingException");
}

// Test that TCLAP's way of handling --version behaves as expected.
TEST_F(OptionsImplTest, Version) {
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{}  --version", client_name_)),
      NoServingException, "NoServingException");
}

// We should fail when no arguments are passed.
TEST_F(OptionsImplTest, NoArguments) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{}", client_name_)),
                          MalformedArgvException, "Required argument missing: uri");
}

TEST_P(OptionsImplIntTestNonZeroable, NonZeroableOptions) {
  const char* option_name = GetParam();
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} {} --{} 0", client_name_,
                                                                     good_test_uri_, option_name)),
                          std::exception, "Proto constraint validation failed");
}

INSTANTIATE_TEST_SUITE_P(NonZeroableIntOptionTests, OptionsImplIntTestNonZeroable,
                         Values("rps", "connections", "duration", "max-active-requests",
                                "max-requests-per-connection"));

// Check standard expectations for any integer values options we offer.
TEST_P(OptionsImplIntTest, IntOptionsBadValuesTest) {
  const char* option_name = GetParam();
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
                         Values("rps", "connections", "duration", "timeout", "request-body-size",
                                "burst-size", "max-pending-requests", "max-active-requests",
                                "max-requests-per-connection"));

// Test behaviour of the boolean valued --h2 flag.
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

// Test --concurrency, which is a bit special. It's an int option, which also accepts 'auto' as
// a value. We need to implement some stuff ourselves to get this to work, hence we don't run it
// through the OptionsImplIntTest.
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

// Test a relatively large uint value to see if we can get reasonable range
// when we specced a uint32_t
// See https://github.com/envoyproxy/nighthawk/pull/88/files#r299572672
TEST_F(OptionsImplTest, ParserIntRangeTest) {
  const uint32_t test_value = OptionsImpl::largest_acceptable_uint32_option_value;
  std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(fmt::format(
      "{} --max-requests-per-connection {}  {} ", client_name_, test_value, good_test_uri_));
  EXPECT_EQ(test_value, options->maxRequestsPerConnection());
}

// Test we accept --concurrency auto
TEST_F(OptionsImplTest, AutoConcurrencyValueParsedOK) {
  std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(
      fmt::format("{} --concurrency auto {} ", client_name_, good_test_uri_));
  EXPECT_EQ("auto", options->concurrency());
}

class OptionsImplVerbosityTest : public OptionsImplTest, public WithParamInterface<const char*> {};

// Test we accept all possible --verbosity values.
TEST_P(OptionsImplVerbosityTest, VerbosityValues) {
  TestUtility::createOptionsImpl(
      fmt::format("{} --verbosity {} {}", client_name_, GetParam(), good_test_uri_));
}

INSTANTIATE_TEST_SUITE_P(VerbosityOptionTests, OptionsImplVerbosityTest,
                         Values("trace", "debug", "info", "warn", "error", "critical"));

// Test we don't accept any bad --verbosity values.
TEST_F(OptionsImplTest, VerbosityValuesAreConstrained) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} {} --verbosity foo", client_name_, good_test_uri_)),
                          MalformedArgvException, "Value 'foo' does not meet constraint");
}

/// ---
class OptionsImplRequestMethodTest : public OptionsImplTest,
                                     public WithParamInterface<const char*> {};

// Test we accept all possible --request-method values.
TEST_P(OptionsImplRequestMethodTest, RequestMethodValues) {
  TestUtility::createOptionsImpl(
      fmt::format("{} --request-method {} {}", client_name_, GetParam(), good_test_uri_));
}

INSTANTIATE_TEST_SUITE_P(RequestMethodOptionTests, OptionsImplRequestMethodTest,
                         Values("GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS",
                                "TRACE"));

// Test we don't accept any bad --request-method values.
TEST_F(OptionsImplTest, RequestMethodValuesAreConstrained) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} {} --request-method foo",
                                                                     client_name_, good_test_uri_)),
                          MalformedArgvException, "Value 'foo' does not meet constraint");
}

class OptionsImplAddressFamilyTest : public OptionsImplTest,
                                     public WithParamInterface<const char*> {};

// Test we accept all possible --address-family values.
TEST_P(OptionsImplAddressFamilyTest, AddressFamilyValues) {
  TestUtility::createOptionsImpl(
      fmt::format("{} --address-family {} {}", client_name_, GetParam(), good_test_uri_));
}

INSTANTIATE_TEST_SUITE_P(AddressFamilyOptionTests, OptionsImplAddressFamilyTest,
                         Values("v4", "v6", "auto"));

// Test we don't accept any bad --address-family values.
TEST_F(OptionsImplTest, AddressFamilyValuesAreConstrained) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} {} --address-family foo",
                                                                     client_name_, good_test_uri_)),
                          MalformedArgvException, "Value 'foo' does not meet constraint");
}

// TODO(oschaaf): URI parsing/validation is weaker then it should be at the moment.
TEST_F(OptionsImplTest, InacceptibleUri) {
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} bad://127.0.0.1/", client_name_)),
      MalformedArgvException, "Invalid URI");
}

TEST_F(OptionsImplTest, ProtoConstructorValidation) {
  const auto option =
      TestUtility::createOptionsImpl(fmt::format("{} http://127.0.0.1/", client_name_));
  auto proto = option->toCommandLineOptions();
  proto->mutable_requests_per_second()->set_value(0);
  EXPECT_THROW_WITH_REGEX(std::make_unique<OptionsImpl>(*proto), MalformedArgvException,
                          "CommandLineOptionsValidationError.RequestsPerSecond");
}

TEST_F(OptionsImplTest, BadTlsContextSpecification) {
  // Bad JSON
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --tls-context {} http://foo/", client_name_, "{broken_json:")),
                          MalformedArgvException, "Unable to parse JSON as proto");
  // Correct JSON, but contents not according to spec.
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} --tls-context {} http://foo/", client_name_,
                                                 "{misspelled_tls_context:{}}")),
      MalformedArgvException, "envoy.api.v2.auth.UpstreamTlsContext reason INVALID_ARGUMENT");
}

class OptionsImplPredicateBasedOptionsTest : public OptionsImplTest,
                                             public WithParamInterface<const char*> {};

TEST_P(OptionsImplPredicateBasedOptionsTest, BadPredicates) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --{} {} http://foo/", client_name_, GetParam(), "a:b:c")),
                          MalformedArgvException,
                          "Termination predicate 'a:b:c' is badly formatted");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --{} {} http://foo/", client_name_, GetParam(), "a:-1")),
                          MalformedArgvException,
                          "Termination predicate 'a:-1' has an out of range threshold");
}

INSTANTIATE_TEST_SUITE_P(PredicateBasedOptionsTest, OptionsImplPredicateBasedOptionsTest,
                         Values("termination-predicate", "failure-predicate"));

class OptionsImplSequencerIdleStrategyTest : public OptionsImplTest,
                                             public WithParamInterface<const char*> {};

// Test we accept all possible --sequencer-idle-strategy values.
TEST_P(OptionsImplSequencerIdleStrategyTest, SequencerIdleStrategyValues) {
  TestUtility::createOptionsImpl(
      fmt::format("{} --sequencer-idle-strategy {} {}", client_name_, GetParam(), good_test_uri_));
}

INSTANTIATE_TEST_SUITE_P(SequencerIdleStrategyOptionsTest, OptionsImplSequencerIdleStrategyTest,
                         Values("sleep", "poll", "spin"));

// Test we don't accept any bad -sequencer-idle-strategy values.
TEST_F(OptionsImplTest, SequencerIdleStrategyValuesAreConstrained) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} {} --sequencer-idle-strategy foo", client_name_, good_test_uri_)),
                          MalformedArgvException, "--sequencer-idle-strategy");
}

} // namespace Client
} // namespace Nighthawk
