#include "external/envoy/test/test_common/utility.h"

#include "client/options_impl.h"

#include "test/client/utility.h"
#include "test/test_common/environment.h"

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

  void verifyHeaderOptionParse(absl::string_view header_option, absl::string_view expected_key,
                               absl::string_view expected_value) {
    std::string s_header_option(header_option);
    std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(std::vector<const char*>{
        client_name_.c_str(), "--request-header", s_header_option.c_str(), good_test_uri_.c_str()});
    EXPECT_EQ(std::vector<std::string>{s_header_option}, options->requestHeaders());
    auto optionsPtr = options->toCommandLineOptions();
    const auto& headers = optionsPtr->request_options().request_headers();
    EXPECT_EQ(1, headers.size());
    EXPECT_EQ(expected_key, headers[0].header().key());
    EXPECT_EQ(expected_value, headers[0].header().value());
  }
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
                          MalformedArgvException, "Invalid target URI: ''");
}

TEST_F(OptionsImplTest, BogusRequestSource) {
  // Request source that looks like an accidental --arg
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} --request-source --foo http://foo", client_name_)),
                          MalformedArgvException, "Invalid replay source URI");
  // Request source that specifies a bad scheme
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --request-source http://bar http://foo", client_name_)),
                          MalformedArgvException, "Invalid replay source URI");
}

TEST_F(OptionsImplTest, NoDurationAndDurationAreMutuallyExclusive) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --duration 5 --no-duration http://foo", client_name_)),
                          MalformedArgvException, "mutually exclusive");
}

TEST_F(OptionsImplTest, DurationAndNoDurationSanity) {
  std::unique_ptr<OptionsImpl> options =
      TestUtility::createOptionsImpl(fmt::format("{} http://foo", client_name_));
  EXPECT_FALSE(options->noDuration());
  EXPECT_EQ(5s, options->duration());

  CommandLineOptionsPtr cmd = options->toCommandLineOptions();
  EXPECT_FALSE(cmd->has_no_duration());
  ASSERT_TRUE(cmd->has_duration());
  EXPECT_EQ(5, cmd->duration().seconds());

  options =
      TestUtility::createOptionsImpl(fmt::format("{} --no-duration http://foo", client_name_));
  EXPECT_TRUE(options->noDuration());
  cmd = options->toCommandLineOptions();
  ASSERT_TRUE(cmd->has_no_duration());
  EXPECT_TRUE(cmd->no_duration().value());
}

TEST_F(OptionsImplTest, StatsSinksMustBeSetWhenStatsFlushIntervalSet) {
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} --stats-flush-interval 10", client_name_)),
      MalformedArgvException,
      "if --stats-flush-interval is set, then --stats-sinks must also be set");
}

// This test should cover every option we offer, except some mutually exclusive ones that
// have separate tests.
TEST_F(OptionsImplTest, AlmostAll) {
  Envoy::MessageUtil util;
  const std::string sink_json_1 =
      "{name:\"envoy.stat_sinks.statsd\",typed_config:{\"@type\":\"type."
      "googleapis.com/"
      "envoy.config.metrics.v3.StatsdSink\",tcp_cluster_name:\"statsd\"}}";
  const std::string sink_json_2 =
      "{name:\"envoy.stat_sinks.statsd\",typed_config:{\"@type\":\"type."
      "googleapis.com/"
      "envoy.config.metrics.v3.StatsdSink\",tcp_cluster_name:\"statsd\",prefix:"
      "\"nighthawk\"}}";

  std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(fmt::format(
      "{} --rps 4 --connections 5 --duration 6 --timeout 7 --h2 "
      "--concurrency 8 --verbosity error --output-format yaml --prefetch-connections "
      "--burst-size 13 --address-family v6 --request-method POST --request-body-size 1234 "
      "--transport-socket {} "
      "--request-header f1:b1 --request-header f2:b2 --request-header f3:b3:b4 "
      "--max-pending-requests 10 "
      "--max-active-requests 11 --max-requests-per-connection 12 --sequencer-idle-strategy sleep "
      "--termination-predicate t1:1 --termination-predicate t2:2 --failure-predicate f1:1 "
      "--failure-predicate f2:2 --jitter-uniform .00001s "
      "--experimental-h2-use-multiple-connections "
      "--experimental-h1-connection-reuse-strategy lru --label label1 --label label2 {} "
      "--simple-warmup --stats-sinks {} --stats-sinks {} --stats-flush-interval 10 "
      "--latency-response-header-name zz",
      client_name_,
      "{name:\"envoy.transport_sockets.tls\","
      "typed_config:{\"@type\":\"type.googleapis.com/"
      "envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext\","
      "common_tls_context:{tls_params:{"
      "cipher_suites:[\"-ALL:ECDHE-RSA-AES256-GCM-SHA384\"]}}}}",
      good_test_uri_, sink_json_1, sink_json_2));

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
  EXPECT_EQ(envoy::config::core::v3::RequestMethod::POST, options->requestMethod());
  const std::vector<std::string> expected_headers = {"f1:b1", "f2:b2", "f3:b3:b4"};
  EXPECT_EQ(expected_headers, options->requestHeaders());
  EXPECT_EQ(1234, options->requestBodySize());
  EXPECT_EQ(
      "name: \"envoy.transport_sockets.tls\"\n"
      "typed_config {\n"
      "  [type.googleapis.com/envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext] {\n"
      "    common_tls_context {\n"
      "      tls_params {\n"
      "        cipher_suites: \"-ALL:ECDHE-RSA-AES256-GCM-SHA384\"\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}\n"
      "183412668: \"envoy.api.v2.core.TransportSocket\"\n",
      options->transportSocket().value().DebugString());
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
  EXPECT_EQ(10us, options->jitterUniform());
  EXPECT_EQ(true, options->h2UseMultipleConnections());
  EXPECT_EQ(nighthawk::client::H1ConnectionReuseStrategy::LRU,
            options->h1ConnectionReuseStrategy());
  const std::vector<std::string> expected_labels{"label1", "label2"};
  EXPECT_EQ(expected_labels, options->labels());
  EXPECT_TRUE(options->simpleWarmup());
  EXPECT_EQ(10, options->statsFlushInterval());
  ASSERT_EQ(2, options->statsSinks().size());
  EXPECT_EQ("name: \"envoy.stat_sinks.statsd\"\n"
            "typed_config {\n"
            "  [type.googleapis.com/envoy.config.metrics.v3.StatsdSink] {\n"
            "    tcp_cluster_name: \"statsd\"\n"
            "  }\n"
            "}\n"
            "183412668: \"envoy.config.metrics.v2.StatsSink\"\n",
            options->statsSinks()[0].DebugString());
  EXPECT_EQ("name: \"envoy.stat_sinks.statsd\"\n"
            "typed_config {\n"
            "  [type.googleapis.com/envoy.config.metrics.v3.StatsdSink] {\n"
            "    tcp_cluster_name: \"statsd\"\n"
            "    prefix: \"nighthawk\"\n"
            "  }\n"
            "}\n"
            "183412668: \"envoy.config.metrics.v2.StatsSink\"\n",
            options->statsSinks()[1].DebugString());
  EXPECT_EQ("zz", options->responseHeaderWithLatencyInput());

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
  EXPECT_TRUE(util(cmd->transport_socket(), options->transportSocket().value()));
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
  EXPECT_EQ(cmd->jitter_uniform().nanos(), options->jitterUniform().count());
  EXPECT_EQ(cmd->experimental_h2_use_multiple_connections().value(),
            options->h2UseMultipleConnections());
  EXPECT_EQ(cmd->experimental_h1_connection_reuse_strategy().value(),
            options->h1ConnectionReuseStrategy());
  EXPECT_THAT(cmd->labels(), ElementsAreArray(expected_labels));
  EXPECT_EQ(cmd->simple_warmup().value(), options->simpleWarmup());
  EXPECT_EQ(10, cmd->stats_flush_interval().value());
  ASSERT_EQ(cmd->stats_sinks_size(), options->statsSinks().size());
  EXPECT_TRUE(util(cmd->stats_sinks(0), options->statsSinks()[0]));
  EXPECT_TRUE(util(cmd->stats_sinks(1), options->statsSinks()[1]));
  EXPECT_EQ(cmd->latency_response_header_name().value(), options->responseHeaderWithLatencyInput());
  // TODO(#433) Here and below, replace comparisons once we choose a proto diff.
  OptionsImpl options_from_proto(*cmd);
  std::string s1 = Envoy::MessageUtil::getYamlStringFromMessage(
      *(options_from_proto.toCommandLineOptions()), true, true);
  std::string s2 = Envoy::MessageUtil::getYamlStringFromMessage(*cmd, true, true);

  EXPECT_EQ(s1, s2);
  // For good measure, also directly test for proto equivalence, though this should be
  // superfluous.
  EXPECT_TRUE(util(*(options_from_proto.toCommandLineOptions()), *cmd));
}

// We test RequestSource here and not in All above because it is exclusive to some of the other
// options.
TEST_F(OptionsImplTest, RequestSource) {
  Envoy::MessageUtil util;
  const std::string request_source = "127.9.9.4:32323";
  std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(
      fmt::format("{} --request-source {} {}", client_name_, request_source, good_test_uri_));
  EXPECT_EQ(options->requestSource(), request_source);
  // Check that our conversion to CommandLineOptionsPtr makes sense.
  CommandLineOptionsPtr cmd = options->toCommandLineOptions();
  EXPECT_EQ(cmd->request_source().uri(), request_source);
  // TODO(#433)
  OptionsImpl options_from_proto(*cmd);
  EXPECT_TRUE(util(*(options_from_proto.toCommandLineOptions()), *cmd));
}

class RequestSourcePluginTestFixture : public OptionsImplTest,
                                       public WithParamInterface<std::string> {};
TEST_P(RequestSourcePluginTestFixture, CreatesOptionsImplWithRequestSourceConfig) {
  Envoy::MessageUtil util;
  const std::string request_source_config = GetParam();
  std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(
      fmt::format("{} --request-source-plugin-config {} {}", client_name_, request_source_config,
                  good_test_uri_));

  CommandLineOptionsPtr command = options->toCommandLineOptions();
  EXPECT_TRUE(
      util(command->request_source_plugin_config(), options->requestSourcePluginConfig().value()));

  // The predicates are defined as proto maps, and these seem to re-serialize into a different
  // order. Hence we trim the maps to contain a single entry so they don't thwart our textual
  // comparison below.
  EXPECT_EQ(1, command->mutable_failure_predicates()->erase("benchmark.http_4xx"));
  EXPECT_EQ(1, command->mutable_failure_predicates()->erase("benchmark.http_5xx"));
  EXPECT_EQ(1, command->mutable_failure_predicates()->erase("benchmark.stream_resets"));
  EXPECT_EQ(1, command->mutable_failure_predicates()->erase("requestsource.upstream_rq_5xx"));

  // TODO(#433)
  // Now we construct a new options from the proto we created above. This should result in an
  // OptionsImpl instance equivalent to options. We test that by converting both to yaml strings,
  // expecting them to be equal. This should provide helpful output when the test fails by showing
  // the unexpected (yaml) diff.
  OptionsImpl options_from_proto(*command);
  std::string yaml_for_options_proto = Envoy::MessageUtil::getYamlStringFromMessage(
      *(options_from_proto.toCommandLineOptions()), true, true);
  std::string yaml_for_command = Envoy::MessageUtil::getYamlStringFromMessage(*command, true, true);
  EXPECT_EQ(yaml_for_options_proto, yaml_for_command);
  // Additional comparison to avoid edge cases missed.
  EXPECT_TRUE(util(*(options_from_proto.toCommandLineOptions()), *command));
}
std::vector<std::string> RequestSourcePluginJsons() {
  std::string file_request_source_plugin_json =
      "{"
      R"(name:"nighthawk.file-based-request-source-plugin",)"
      "typed_config:{"
      R"("@type":"type.googleapis.com/)"
      R"(nighthawk.request_source.FileBasedOptionsListRequestSourceConfig",)"
      R"(file_path:")" +
      TestEnvironment::runfilesPath("test/request_source/test_data/test-config.yaml") +
      "\","
      "}"
      "}";
  std::string in_line_request_source_plugin_json =
      "{"
      R"(name:"nighthawk.in-line-options-list-request-source-plugin",)"
      "typed_config:{"
      R"("@type":"type.googleapis.com/)"
      R"(nighthawk.request_source.InLineOptionsListRequestSourceConfig",)"
      "options_list:{"
      R"(options:[{request_method:"1",request_headers:[{header:{key:"key",value:"value"}}]}])"
      "},"
      "}"
      "}";
  std::string stub_request_source_plugin_json =
      "{"
      R"(name:"nighthawk.stub-request-source-plugin",)"
      "typed_config:{"
      R"("@type":"type.googleapis.com/nighthawk.request_source.StubPluginConfig",)"
      R"(test_value:"3",)"
      "}"
      "}";
  return std::vector<std::string>{
      file_request_source_plugin_json,
      in_line_request_source_plugin_json,
      stub_request_source_plugin_json,
  };
}
INSTANTIATE_TEST_SUITE_P(HappyPathRequestSourceConfigJsonSuccessfullyTranslatesIntoOptions,
                         RequestSourcePluginTestFixture,
                         testing::ValuesIn(RequestSourcePluginJsons()));

// This test covers --RequestSourcePlugin, which can't be tested at the same time as --RequestSource
// and some other options. This is the test for the inlineoptionslistplugin.
TEST_F(OptionsImplTest, InLineOptionsListRequestSourcePluginIsMutuallyExclusiveWithRequestSource) {
  const std::string request_source = "127.9.9.4:32323";
  const std::string request_source_config =
      "{"
      "name:\"nighthawk.in-line-options-list-request-source-plugin\","
      "typed_config:{"
      "\"@type\":\"type.googleapis.com/"
      "nighthawk.request_source.InLineOptionsListRequestSourceConfig\","
      "options_list:{"
      "options:[{request_method:\"1\",request_headers:[{header:{key:\"key\",value:\"value\"}}]}]"
      "},"
      "}"
      "}";
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(
          fmt::format("{} --request-source-plugin-config {} --request-source {} {}", client_name_,
                      request_source_config, request_source, good_test_uri_)),
      MalformedArgvException,
      "--request-source and --request_source_plugin_config cannot both be set.");
}

TEST_F(OptionsImplTest, BadRequestSourcePluginSpecification) {
  // Bad JSON
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} --request-source-plugin-config {} {}",
                                                 client_name_, "{broken_json:", good_test_uri_)),
      MalformedArgvException, "Unable to parse JSON as proto");
  // Correct JSON, but contents not according to spec.
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} --request-source-plugin-config {} {}", client_name_,
                                          "{misspelled_field:{}}", good_test_uri_)),
                          MalformedArgvException,
                          "envoy.config.core.v3.TypedExtensionConfig reason INVALID_ARGUMENT");
}

// We test --no-duration here and not in All above because it is exclusive to --duration.
TEST_F(OptionsImplTest, NoDuration) {
  Envoy::MessageUtil util;
  std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(
      fmt::format("{} --no-duration {}", client_name_, good_test_uri_));
  EXPECT_TRUE(options->noDuration());
  // Check that our conversion to CommandLineOptionsPtr makes sense.
  CommandLineOptionsPtr cmd = options->toCommandLineOptions();
  // TODO(#433)
  OptionsImpl options_from_proto(*cmd);
  EXPECT_TRUE(util(*(options_from_proto.toCommandLineOptions()), *cmd));
}

// This test covers --tls-context, which can't be tested at the same time as --transport-socket.
// We test --tls-context here and not in AlmostAll above because it is mutually
// exclusive with --transport-socket.
TEST_F(OptionsImplTest, TlsContext) {
  Envoy::MessageUtil util;
  std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(
      fmt::format("{} --tls-context {} {}", client_name_,
                  "{common_tls_context:{tls_params:{"
                  "cipher_suites:[\"-ALL:ECDHE-RSA-AES256-GCM-SHA384\"]}}}",
                  good_test_uri_));

  EXPECT_EQ("common_tls_context {\n"
            "  tls_params {\n"
            "    cipher_suites: \"-ALL:ECDHE-RSA-AES256-GCM-SHA384\"\n"
            "    183412668: \"envoy.api.v2.auth.TlsParameters\"\n"
            "  }\n"
            "  183412668: \"envoy.api.v2.auth.CommonTlsContext\"\n"
            "}\n"
            "183412668: \"envoy.api.v2.auth.UpstreamTlsContext\"\n",
            options->tlsContext().DebugString());

  // Check that our conversion to CommandLineOptionsPtr makes sense.
  CommandLineOptionsPtr cmd = options->toCommandLineOptions();
  EXPECT_TRUE(util(cmd->tls_context(), options->tlsContext()));

  // Now we construct a new options from the proto we created above. This should result in an
  // OptionsImpl instance equivalent to options. We test that by converting both to yaml strings,
  // expecting them to be equal. This should provide helpful output when the test fails by showing
  // the unexpected (yaml) diff.

  // The predicates are defined as proto maps, and these seem to re-serialize into a different
  // order. Hence we trim the maps to contain a single entry so they don't thwart our textual
  // comparison below.
  EXPECT_EQ(1, cmd->mutable_failure_predicates()->erase("benchmark.http_4xx"));
  EXPECT_EQ(1, cmd->mutable_failure_predicates()->erase("benchmark.http_5xx"));
  EXPECT_EQ(1, cmd->mutable_failure_predicates()->erase("benchmark.stream_resets"));
  EXPECT_EQ(1, cmd->mutable_failure_predicates()->erase("requestsource.upstream_rq_5xx"));
  // TODO(#433)
  OptionsImpl options_from_proto(*cmd);
  std::string s1 = Envoy::MessageUtil::getYamlStringFromMessage(
      *(options_from_proto.toCommandLineOptions()), true, true);
  std::string s2 = Envoy::MessageUtil::getYamlStringFromMessage(*cmd, true, true);

  EXPECT_EQ(s1, s2);
  // For good measure, also directly test for proto equivalence, though this should be
  // superfluous.
  EXPECT_TRUE(util(*(options_from_proto.toCommandLineOptions()), *cmd));
}

// We test --multi-target-* options here and not in AlmostAll above because they are mutually
// exclusive with the URI arg.
TEST_F(OptionsImplTest, MultiTarget) {
  Envoy::MessageUtil util;
  std::unique_ptr<OptionsImpl> options = TestUtility::createOptionsImpl(
      fmt::format("{} --multi-target-endpoint 1.1.1.1:3 "
                  "--multi-target-endpoint 2.2.2.2:4 "
                  "--multi-target-endpoint [::1]:5 "
                  "--multi-target-endpoint www.example.com:6 "
                  "--multi-target-path /x/y/z --multi-target-use-https",
                  client_name_));

  EXPECT_EQ("/x/y/z", options->multiTargetPath());
  EXPECT_EQ(true, options->multiTargetUseHttps());

  ASSERT_EQ(4, options->multiTargetEndpoints().size());
  EXPECT_EQ("1.1.1.1", options->multiTargetEndpoints()[0].address().value());
  EXPECT_EQ(3, options->multiTargetEndpoints()[0].port().value());
  EXPECT_EQ("2.2.2.2", options->multiTargetEndpoints()[1].address().value());
  EXPECT_EQ(4, options->multiTargetEndpoints()[1].port().value());
  EXPECT_EQ("[::1]", options->multiTargetEndpoints()[2].address().value());
  EXPECT_EQ(5, options->multiTargetEndpoints()[2].port().value());
  EXPECT_EQ("www.example.com", options->multiTargetEndpoints()[3].address().value());
  EXPECT_EQ(6, options->multiTargetEndpoints()[3].port().value());

  CommandLineOptionsPtr cmd = options->toCommandLineOptions();

  EXPECT_EQ(cmd->multi_target().use_https().value(), options->multiTargetUseHttps());
  EXPECT_EQ(cmd->multi_target().path().value(), options->multiTargetPath());

  ASSERT_EQ(4, cmd->multi_target().endpoints_size());
  EXPECT_EQ(cmd->multi_target().endpoints(0).address().value(), "1.1.1.1");
  EXPECT_EQ(cmd->multi_target().endpoints(0).port().value(), 3);
  EXPECT_EQ(cmd->multi_target().endpoints(1).address().value(), "2.2.2.2");
  EXPECT_EQ(cmd->multi_target().endpoints(1).port().value(), 4);
  EXPECT_EQ(cmd->multi_target().endpoints(2).address().value(), "[::1]");
  EXPECT_EQ(cmd->multi_target().endpoints(2).port().value(), 5);
  EXPECT_EQ(cmd->multi_target().endpoints(3).address().value(), "www.example.com");
  EXPECT_EQ(cmd->multi_target().endpoints(3).port().value(), 6);

  // Now we construct a new options from the proto we created above. This should result in an
  // OptionsImpl instance equivalent to options. We test that by converting both to yaml strings,
  // expecting them to be equal. This should provide helpful output when the test fails by showing
  // the unexpected (yaml) diff.
  // The predicates are defined as proto maps, and these seem to re-serialize into a different
  // order. Hence we trim the maps to contain a single entry so they don't thwart our
  // textual comparison below.
  EXPECT_EQ(1, cmd->mutable_failure_predicates()->erase("benchmark.http_4xx"));
  EXPECT_EQ(1, cmd->mutable_failure_predicates()->erase("benchmark.http_5xx"));
  EXPECT_EQ(1, cmd->mutable_failure_predicates()->erase("benchmark.stream_resets"));
  EXPECT_EQ(1, cmd->mutable_failure_predicates()->erase("requestsource.upstream_rq_5xx"));
  // TODO(#433)
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
                          MalformedArgvException,
                          "A URI or --multi-target-\\* options must be specified.");
}

TEST_P(OptionsImplIntTestNonZeroable, NonZeroableOptions) {
  const char* option_name = GetParam();
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --{} 0 --stats-sinks {} {}", client_name_, option_name,
                              "{name:\"envoy.stat_sinks.statsd\"}", good_test_uri_)),
                          std::exception, "Proto constraint validation failed");
}

INSTANTIATE_TEST_SUITE_P(NonZeroableIntOptionTests, OptionsImplIntTestNonZeroable,
                         Values("rps", "connections", "max-active-requests",
                                "max-requests-per-connection", "stats-flush-interval"));

// Check standard expectations for any integer values options we offer.
TEST_P(OptionsImplIntTest, IntOptionsBadValuesTest) {
  const char* option_name = GetParam();
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} --{} -1 {}", client_name_,
                                                                     option_name, good_test_uri_)),
                          MalformedArgvException,
                          fmt::format("Invalid value for --{}", option_name));
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} --{} {}", client_name_, option_name, good_test_uri_)),
                          MalformedArgvException, "Couldn't read argument value from string");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} --{} foo {}", client_name_,
                                                                     option_name, good_test_uri_)),
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
      TestUtility::createOptionsImpl(fmt::format("{} --h2 {}", client_name_, good_test_uri_))
          ->h2());
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} --h2 0 {}", client_name_, good_test_uri_)),
      MalformedArgvException, "Couldn't find match for argument");
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} --h2 true {}", client_name_, good_test_uri_)),
      MalformedArgvException, "Couldn't find match for argument");
}

TEST_F(OptionsImplTest, PrefetchConnectionsFlag) {
  EXPECT_FALSE(TestUtility::createOptionsImpl(fmt::format("{} {}", client_name_, good_test_uri_))
                   ->prefetchConnections());
  EXPECT_TRUE(TestUtility::createOptionsImpl(
                  fmt::format("{} --prefetch-connections {}", client_name_, good_test_uri_))
                  ->prefetchConnections());
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --prefetch-connections 0 {}", client_name_, good_test_uri_)),
                          MalformedArgvException, "Couldn't find match for argument");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --prefetch-connections true {}", client_name_, good_test_uri_)),
                          MalformedArgvException, "Couldn't find match for argument");
}

// Test --concurrency, which is a bit special. It's an int option, which also accepts 'auto' as
// a value. We need to implement some stuff ourselves to get this to work, hence we don't run it
// through the OptionsImplIntTest.
TEST_F(OptionsImplTest, BadConcurrencyValuesThrow) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} --concurrency 0 {}", client_name_, good_test_uri_)),
                          MalformedArgvException,
                          "Value for --concurrency should be greater then 0.");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} --concurrency -1 {}", client_name_, good_test_uri_)),
                          MalformedArgvException,
                          "Value for --concurrency should be greater then 0.");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} --concurrency foo {}", client_name_, good_test_uri_)),
                          MalformedArgvException, "Invalid value for --concurrency");
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(
          fmt::format("{} --concurrency 999999999999999999999 {}", client_name_, good_test_uri_)),
      MalformedArgvException, "Value out of range: --concurrency");
}

TEST_F(OptionsImplTest, JitterValueRangeTest) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} --jitter-uniform a {}",
                                                                     client_name_, good_test_uri_)),
                          MalformedArgvException, "Invalid value for --jitter-uniform");
  // Should end with 's'.
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} --jitter-uniform .1 {}",
                                                                     client_name_, good_test_uri_)),
                          MalformedArgvException, "Invalid value for --jitter-uniform");
  // No negative durations accepted
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} --jitter-uniform -1s {}",
                                                                     client_name_, good_test_uri_)),
                          MalformedArgvException, "--jitter-uniform is out of range");
  // Durations >= 0s are accepted
  EXPECT_NO_THROW(TestUtility::createOptionsImpl(
      fmt::format("{} --jitter-uniform 0s {}", client_name_, good_test_uri_)));
  EXPECT_NO_THROW(TestUtility::createOptionsImpl(
      fmt::format("{} --jitter-uniform 0.1s {}", client_name_, good_test_uri_)));
  EXPECT_NO_THROW(TestUtility::createOptionsImpl(
      fmt::format("{} --jitter-uniform 1s {}", client_name_, good_test_uri_)));
  EXPECT_NO_THROW(TestUtility::createOptionsImpl(
      fmt::format("{} --jitter-uniform 100s {}", client_name_, good_test_uri_)));
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
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} --address-family foo {}",
                                                                     client_name_, good_test_uri_)),
                          MalformedArgvException, "Value 'foo' does not meet constraint");
}

// TODO(oschaaf): URI parsing/validation is weaker then it should be at the moment.
TEST_F(OptionsImplTest, InacceptibleUri) {
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} bad://127.0.0.1/", client_name_)),
      MalformedArgvException, "Invalid target URI: ''");
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
      MalformedArgvException,
      "envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext reason INVALID_ARGUMENT");
}

TEST_F(OptionsImplTest, BadTransportSocketSpecification) {
  // Bad JSON
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(
          fmt::format("{} --transport-socket {} http://foo/", client_name_, "{broken_json:")),
      MalformedArgvException, "Unable to parse JSON as proto");
  // Correct JSON, but contents not according to spec.
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} --transport-socket {} http://foo/",
                                                 client_name_, "{misspelled_transport_socket:{}}")),
      MalformedArgvException,
      "Protobuf message \\(type envoy.config.core.v3.TransportSocket reason "
      "INVALID_ARGUMENT:misspelled_transport_socket: Cannot find field.\\) has unknown fields");
}

TEST_F(OptionsImplTest, BadStatsSinksSpecification) {
  // Bad JSON
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --stats-sinks {} http://foo/", client_name_, "{broken_json:")),
                          MalformedArgvException, "Unable to parse JSON as proto");
  // Correct JSON, but contents not according to spec.
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} --stats-sinks {} http://foo/", client_name_,
                                                 "{misspelled_stats_sink:{}}")),
      MalformedArgvException, "misspelled_stats_sink: Cannot find field");
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

TEST_F(OptionsImplTest, RequestHeaderWithoutColon) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format("{} --request-header bar {}",
                                                                     client_name_, good_test_uri_)),
                          MalformedArgvException, "is required in a header");
}

TEST_F(OptionsImplTest, RequestHeaderValueWithColonsAndSpaces) {
  verifyHeaderOptionParse("bar: baz", "bar", "baz");
  verifyHeaderOptionParse("\t\n bar:  baz  \t\n", "bar", "baz");
  verifyHeaderOptionParse(":authority: baz", ":authority", "baz");
}

TEST_F(OptionsImplTest, MultiTargetEndpointMalformed) {
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format(
          "{} --multi-target-path /x/y/z --multi-target-endpoint my.dns.name", client_name_)),
      MalformedArgvException, "must be in the format");

  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format(
          "{} --multi-target-path /x/y/z --multi-target-endpoint my.dns.name:xyz", client_name_)),
      MalformedArgvException, "must be in the format");

  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format(
          "{} --multi-target-path /x/y/z --multi-target-endpoint 1.1.1.1", client_name_)),
      MalformedArgvException, "must be in the format");

  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format(
          "{} --multi-target-path /x/y/z --multi-target-endpoint '[::1]'", client_name_)),
      MalformedArgvException, "must be in the format");

  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format(
          "{} --multi-target-path /x/y/z --multi-target-endpoint a.1:a.1:33", client_name_)),
      MalformedArgvException, "must be in the format");

  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(
          fmt::format("{} --multi-target-path /x/y/z --multi-target-endpoint :0", client_name_)),
      MalformedArgvException, "must be in the format");

  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(
          fmt::format("{} --multi-target-path /x/y/z --multi-target-endpoint :", client_name_)),
      MalformedArgvException, "must be in the format");
}

TEST_F(OptionsImplTest, BothUriAndMultiTargetSpecified) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --multi-target-path /x/y/z --multi-target-endpoint 1.2.3.4:5 {}",
                              client_name_, good_test_uri_)),
                          MalformedArgvException,
                          "URI and --multi-target-\\* options cannot both be specified.");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --multi-target-path /x/y/z {}", client_name_, good_test_uri_)),
                          MalformedArgvException,
                          "URI and --multi-target-\\* options cannot both be specified.");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --multi-target-path /x/y/z --multi-target-use-https {}",
                              client_name_, good_test_uri_)),
                          MalformedArgvException,
                          "URI and --multi-target-\\* options cannot both be specified.");
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(fmt::format(
                              "{} --multi-target-path /x/y/z --multi-target-use-https {}",
                              client_name_, good_test_uri_)),
                          MalformedArgvException,
                          "URI and --multi-target-\\* options cannot both be specified.");
}

TEST_F(OptionsImplTest, IncorrectMultiTargetCombination) {
  EXPECT_THROW_WITH_REGEX(TestUtility::createOptionsImpl(
                              fmt::format("{} --multi-target-endpoint 1.2.3.4:5", client_name_)),
                          MalformedArgvException, "--multi-target-path must be specified.");
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} --multi-target-path /x/y/z", client_name_)),
      MalformedArgvException, "A URI or --multi-target-\\* options must be specified.");
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format("{} --multi-target-use-https", client_name_)),
      MalformedArgvException, "A URI or --multi-target-\\* options must be specified.");
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(
          fmt::format("{} --multi-target-path /x/y/z --multi-target-use-https", client_name_)),
      MalformedArgvException, "A URI or --multi-target-\\* options must be specified.");
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(
          fmt::format("{} --multi-target-path /x/y/z --multi-target-use-https", client_name_)),
      MalformedArgvException, "A URI or --multi-target-\\* options must be specified.");
}

TEST_F(OptionsImplTest, BothTlsContextAndTransportSocketSpecified) {
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(
          fmt::format("{} --tls-context x --transport-socket y {}", client_name_, good_test_uri_)),
      MalformedArgvException, "cannot both be set");
}

class OptionsImplH1ConnectionReuseStrategyTest : public OptionsImplTest,
                                                 public WithParamInterface<const char*> {};

// Test we accept all possible --experimental-h1-connection-reuse-strategy values.
TEST_P(OptionsImplH1ConnectionReuseStrategyTest, H1ConnectionReuseStrategyValues) {
  TestUtility::createOptionsImpl(fmt::format("{} --experimental-h1-connection-reuse-strategy {} {}",
                                             client_name_, GetParam(), good_test_uri_));
}

INSTANTIATE_TEST_SUITE_P(H1ConnectionReuseStrategyOptionsTest,
                         OptionsImplH1ConnectionReuseStrategyTest, Values("mru", "lru"));

// Test we don't accept any bad --experimental-h1-connection-reuse-strategy values.
TEST_F(OptionsImplTest, H1ConnectionReuseStrategyValuesAreConstrained) {
  EXPECT_THROW_WITH_REGEX(
      TestUtility::createOptionsImpl(fmt::format(
          "{} {} --experimental-h1-connection-reuse-strategy foo", client_name_, good_test_uri_)),
      MalformedArgvException, "experimental-h1-connection-reuse-strategy");
}

} // namespace Client
} // namespace Nighthawk
