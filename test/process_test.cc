#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "nighthawk/common/exception.h"

#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/common/statusor.h"
#include "external/envoy/test/mocks/network/mocks.h"
#include "external/envoy/test/test_common/environment.h"
#include "external/envoy/test/test_common/network_utility.h"
#include "external/envoy/test/test_common/registry.h"
#include "external/envoy/test/test_common/simulated_time_system.h"
#include "external/envoy/test/test_common/test_runtime.h"
#include "external/envoy/test/test_common/utility.h"

#include "source/client/options_impl.h"
#include "source/client/output_collector_impl.h"
#include "source/client/process_impl.h"
#include "source/common/uri_impl.h"
#include "source/user_defined_output/log_response_headers_plugin.h"

#include "test/client/utility.h"
#include "test/sink/test_stats_sink_config.pb.h"
#include "test/test_common/proto_matchers.h"
#include "test/user_defined_output/fake_plugin/fake_user_defined_output.h"

#include "gtest/gtest.h"

namespace Nighthawk {
namespace Client {
namespace {

using ::envoy::config::core::v3::TypedExtensionConfig;
using ::Envoy::Protobuf::TextFormat;
using ::testing::HasSubstr;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

constexpr absl::string_view kSinkConfig =
    "{name:\"nighthawk.fake_stats_sink\",typed_config:{\"@type\":\"type."
    "googleapis.com/"
    "nighthawk.TestStatsSinkConfig\"}}";
// Global variable keeps count of number of flushes in FakeStatsSink. It is reset
// to 0 when a new FakeStatsSink is created.
int numFlushes = 0;

// FakeStatsSink is a simple Envoy::Stats::Sink implementation used to prove
// the logic to configure Sink in Nighthawk works as expected.
class FakeStatsSink : public Envoy::Stats::Sink {
public:
  FakeStatsSink() { numFlushes = 0; }

  // Envoy::Stats::Sink
  void flush(Envoy::Stats::MetricSnapshot&) override { numFlushes++; }

  void onHistogramComplete(const Envoy::Stats::Histogram&, uint64_t) override {}
};

// FakeStatsSinkFactory creates FakeStatsSink.
class FakeStatsSinkFactory : public NighthawkStatsSinkFactory {
public:
  std::unique_ptr<Envoy::Stats::Sink> createStatsSink(Envoy::Stats::SymbolTable&) override {
    return std::make_unique<FakeStatsSink>();
  }

  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    // Uses empty config proto.
    return Envoy::ProtobufTypes::MessagePtr{new nighthawk::TestStatsSinkConfig()};
  }

  std::string name() const override { return "nighthawk.fake_stats_sink"; }
};

// WedgedTcpServer is a minimal TCP server which accepts connections but never writes a
// response and never closes accepted sockets. Any HTTP request sent to it therefore remains
// in flight forever. This forces BenchmarkClientHttpImpl::terminate() to wait for the full
// connection pool drain timeout (--timeout) when a worker shuts down.
class WedgedTcpServer {
public:
  explicit WedgedTcpServer(Envoy::Network::Address::IpVersion ip_version) {
    const bool is_v4 = ip_version == Envoy::Network::Address::IpVersion::v4;
    listen_fd_ = ::socket(is_v4 ? AF_INET : AF_INET6, SOCK_STREAM, 0);
    RELEASE_ASSERT(listen_fd_ >= 0, "WedgedTcpServer: socket() failed");
    sockaddr_storage addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t addr_len;
    if (is_v4) {
      auto* addr4 = reinterpret_cast<sockaddr_in*>(&addr);
      addr4->sin_family = AF_INET;
      addr4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      addr4->sin_port = 0;
      addr_len = sizeof(sockaddr_in);
    } else {
      auto* addr6 = reinterpret_cast<sockaddr_in6*>(&addr);
      addr6->sin6_family = AF_INET6;
      addr6->sin6_addr = in6addr_loopback;
      addr6->sin6_port = 0;
      addr_len = sizeof(sockaddr_in6);
    }
    RELEASE_ASSERT(::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), addr_len) == 0,
                   "WedgedTcpServer: bind() failed");
    RELEASE_ASSERT(::listen(listen_fd_, 128) == 0, "WedgedTcpServer: listen() failed");
    addr_len = sizeof(addr);
    RELEASE_ASSERT(::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0,
                   "WedgedTcpServer: getsockname() failed");
    port_ = ntohs(is_v4 ? reinterpret_cast<sockaddr_in*>(&addr)->sin_port
                        : reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port);
    accept_thread_ = std::thread([this]() { acceptLoop(); });
  }

  ~WedgedTcpServer() {
    stopping_ = true;
    accept_thread_.join();
    for (const int fd : accepted_fds_) {
      ::close(fd);
    }
    ::close(listen_fd_);
  }

  uint32_t port() const { return port_; }

private:
  void acceptLoop() {
    while (!stopping_) {
      pollfd poll_fd{listen_fd_, POLLIN, 0};
      const int rc = ::poll(&poll_fd, 1, /*timeout=*/50);
      if (rc > 0 && (poll_fd.revents & POLLIN) != 0) {
        const int fd = ::accept(listen_fd_, nullptr, nullptr);
        if (fd >= 0) {
          // Deliberately never read from or write to the socket, so any request wedges.
          accepted_fds_.push_back(fd);
        }
      }
    }
  }

  int listen_fd_{-1};
  uint32_t port_{0};
  std::atomic<bool> stopping_{false};
  std::vector<int> accepted_fds_;
  std::thread accept_thread_;
};

// TODO(https://github.com/envoyproxy/nighthawk/issues/179): Mock Process in client_test, and move
// it's tests in here. Note: these tests do not have a backend set up to talk to. That's why we
// expect failure.
class ProcessTest : public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  enum class RunExpectation { EXPECT_SUCCESS, EXPECT_FAILURE };

  ProcessTest()
      : loopback_address_(Envoy::Network::Test::getLoopbackAddressUrlString(GetParam())),
        options_(TestUtility::createOptionsImpl(
            fmt::format("foo --duration 1 -v error --rps 10 https://{}/", loopback_address_))) {};

  absl::Status runProcess(RunExpectation expectation, bool do_cancel = false,
                          bool terminate_right_away = false,
                          bool bad_dns_resolver_factory = false) {
    TypedExtensionConfig typed_dns_resolver_config;
    Envoy::Network::DnsResolverFactory& dns_resolver_factory =
        Envoy::Network::createDefaultDnsResolverFactory(typed_dns_resolver_config);
    Envoy::Network::DnsResolverFactory* dns_resolver_factory_ptr = &dns_resolver_factory;
    NiceMock<Envoy::Network::MockDnsResolverFactory> mock_dns_resolver_factory;
    if (bad_dns_resolver_factory) {
      EXPECT_CALL(mock_dns_resolver_factory, createDnsResolver(_, _, _))
          .WillOnce(Invoke(
              [&dns_resolver_factory](
                  Envoy::Event::Dispatcher& dispatcher, Envoy::Api::Api& api,
                  const envoy::config::core::v3::TypedExtensionConfig& typed_dns_resolver_config) {
                return dns_resolver_factory.createDnsResolver(dispatcher, api,
                                                              typed_dns_resolver_config);
              }))
          .WillOnce(testing::Return(absl::InternalError("Test DnsResolverFactory error")));
      dns_resolver_factory_ptr = &mock_dns_resolver_factory;
    }
    absl::StatusOr<ProcessPtr> process_or_status = ProcessImpl::CreateProcessImpl(
        *options_, *dns_resolver_factory_ptr, std::move(typed_dns_resolver_config), time_system_);
    if (!process_or_status.ok()) {
      return process_or_status.status();
    }
    ProcessPtr process = std::move(process_or_status.value());

    OutputCollectorImpl collector(time_system_, *options_);
    std::thread cancel_thread;
    if (do_cancel) {
      cancel_thread = std::thread([&process, terminate_right_away] {
        if (!terminate_right_away) {
          // We sleep to give the the load test execution in the other thread a change to get
          // started before we request cancellation. Five seconds has been determined to work with
          // the sanitizer runs in CI through emperical observation.
          sleep(5);
        }
        process->requestExecutionCancellation();
      });
      if (terminate_right_away) {
        cancel_thread.join();
      }
    }
    const RunExpectation result =
        process->run(collector) ? RunExpectation::EXPECT_SUCCESS : RunExpectation::EXPECT_FAILURE;
    output_proto_ = collector.toProto();
    EXPECT_EQ(result, expectation);
    if (do_cancel) {
      if (cancel_thread.joinable()) {
        cancel_thread.join();
      }
      if (terminate_right_away) {
        EXPECT_EQ(0, output_proto_.results().size());
      } else {
        int graceful_stop_requested = 0;
        for (const auto& result : output_proto_.results()) {
          for (const auto& counter : result.counters()) {
            if (counter.name() == "graceful_stop_requested") {
              graceful_stop_requested++;
            }
          }
        }
        EXPECT_EQ(3, graceful_stop_requested); // global results + two workers
      }
    }
    process->shutdown();
    return absl::OkStatus();
  }

  const std::string loopback_address_;
  OptionsPtr options_;
  nighthawk::client::Output output_proto_;
  Envoy::Event::RealTimeSystem time_system_; // NO_CHECK_FORMAT(real_time)
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ProcessTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(ProcessTest, FailsToCreateProcessOnUnresolvableHost) {
  options_ =
      TestUtility::createOptionsImpl("foo --h2 --duration 1 --rps 10 https://unresolveable.host/");
  EXPECT_FALSE(runProcess(RunExpectation::EXPECT_FAILURE).ok());
}

TEST_P(ProcessTest, TwoProcessInSequence) {
  ASSERT_TRUE(runProcess(RunExpectation::EXPECT_FAILURE).ok());
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --h2 --duration 1 --rps 10 https://{}/", loopback_address_));
  EXPECT_TRUE(runProcess(RunExpectation::EXPECT_FAILURE).ok());
}

// TODO(oschaaf): move to python int. tests once it adds to coverage.
TEST_P(ProcessTest, BadTracerSpec) {
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --trace foo://localhost:79/api/v2/spans https://{}/", loopback_address_));
  EXPECT_TRUE(runProcess(RunExpectation::EXPECT_FAILURE).ok());
}

TEST_P(ProcessTest, CancelDuringLoadTest) {
  // The failure predicate below is there to wipe out any stock ones. We want this to run for a long
  // time, even if the upstream fails (there is no live upstream in this test, we send traffic into
  // the void), so we can check cancellation works.
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --duration 300 --failure-predicate foo:0 --concurrency 2 https://{}/",
                  loopback_address_));
  EXPECT_TRUE(runProcess(RunExpectation::EXPECT_SUCCESS, /*do_cancel=*/true).ok());
}

TEST_P(ProcessTest, CancelExecutionBeforeBeginLoadTest) {
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --duration 300 --failure-predicate foo:0 --concurrency 2 https://{}/",
                  loopback_address_));
  EXPECT_TRUE(
      runProcess(RunExpectation::EXPECT_SUCCESS, /*do_cancel=*/true, /*terminate_right_away=*/true)
          .ok());
}

TEST_P(ProcessTest, RunProcessWithStatsSinkConfigured) {
  FakeStatsSinkFactory factory;
  Envoy::Registry::InjectFactory<NighthawkStatsSinkFactory> registered(factory);
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --h2 --duration 1 --rps 10 --stats-flush-interval 1 "
                  "--stats-sinks {} https://{}/",
                  kSinkConfig, loopback_address_));
  numFlushes = 0;
  ASSERT_TRUE(runProcess(RunExpectation::EXPECT_FAILURE).ok());
  EXPECT_GT(numFlushes, 0);
}

TEST_P(ProcessTest, NoFlushWhenCancelExecutionBeforeLoadTestBegin) {
  FakeStatsSinkFactory factory;
  Envoy::Registry::InjectFactory<NighthawkStatsSinkFactory> registered(factory);
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --duration 300 --failure-predicate foo:0 --concurrency "
                  "2 --stats-flush-interval 1 --stats-sinks {} https://{}/",
                  kSinkConfig, loopback_address_));
  numFlushes = 0;
  ASSERT_TRUE(
      runProcess(RunExpectation::EXPECT_SUCCESS, /*do_cancel=*/true, /*terminate_right_away=*/true)
          .ok());
  EXPECT_EQ(numFlushes, 0);
}

TEST_P(ProcessTest, CreatesUserDefinedOutputPluginPerWorkerThread) {
  FakeUserDefinedOutputPluginFactory factory;
  Envoy::Registry::InjectFactory<UserDefinedOutputPluginFactory> registered(factory);
  const std::string user_defined_output_plugin =
      "{name:\"nighthawk.fake_user_defined_output\",typed_config:"
      "{\"@type\":\"type.googleapis.com/nighthawk.FakeUserDefinedOutputConfig\"}}";
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --concurrency 2 --user-defined-plugin-config {} https://{}/",
                  user_defined_output_plugin, loopback_address_));

  // Expect connection failure, but ensure that User Defined Outputs were set up correctly.
  EXPECT_TRUE(runProcess(RunExpectation::EXPECT_FAILURE).ok());
  EXPECT_EQ(factory.getPluginCount(), 2);
}

TEST_P(ProcessTest, ReturnsUserDefinedOutputsInResults) {
  const std::string fake_plugin =
      "{name:\"nighthawk.fake_user_defined_output\",typed_config:"
      "{\"@type\":\"type.googleapis.com/nighthawk.FakeUserDefinedOutputConfig\"}}";
  const std::string logging_plugin =
      "{name:\"nighthawk.log_response_headers_plugin\",typed_config:"
      "{\"@type\":\"type.googleapis.com/nighthawk.LogResponseHeadersConfig\","
      "logging_mode:\"LM_LOG_ALL_RESPONSES\"}}";
  options_ =
      TestUtility::createOptionsImpl(fmt::format("foo --concurrency 1 --user-defined-plugin-config "
                                                 "{} --user-defined-plugin-config {} https://{}/",
                                                 fake_plugin, logging_plugin, loopback_address_));

  // Expect connection failure, but ensure that User Defined Outputs were set up correctly.
  EXPECT_TRUE(runProcess(RunExpectation::EXPECT_FAILURE).ok());

  nighthawk::client::UserDefinedOutput expected_fake_user_defined_output;
  std::ignore = TextFormat::ParseFromString(R"(plugin_name: "nighthawk.fake_user_defined_output"
                                 typed_output {
                                   [type.googleapis.com/nighthawk.FakeUserDefinedOutput] {
                                     worker_name: "global"
                                   }
                                 }
                                 )",
                                            &expected_fake_user_defined_output);
  nighthawk::client::UserDefinedOutput expected_logging_output;
  std::ignore = TextFormat::ParseFromString(R"(plugin_name: "nighthawk.log_response_headers_plugin"
                                 typed_output {
                                   [type.googleapis.com/nighthawk.LogResponseHeadersOutput] {}
                                 }
                                 )",
                                            &expected_logging_output);

  ASSERT_EQ(output_proto_.results_size(), 1);
  const nighthawk::client::Result& result = output_proto_.results(0);
  ASSERT_EQ(result.user_defined_outputs_size(), 2);
  const nighthawk::client::UserDefinedOutput actual_fake_output =
      result.user_defined_outputs(0).plugin_name() == "nighthawk.fake_user_defined_output"
          ? result.user_defined_outputs(0)
          : result.user_defined_outputs(1);
  const nighthawk::client::UserDefinedOutput actual_logging_output =
      result.user_defined_outputs(0).plugin_name() == "nighthawk.log_response_headers_plugin"
          ? result.user_defined_outputs(0)
          : result.user_defined_outputs(1);
  EXPECT_THAT(actual_fake_output, EqualsProto(expected_fake_user_defined_output));
  EXPECT_THAT(actual_logging_output, EqualsProto(expected_logging_output));
}

TEST_P(ProcessTest, ReturnsUserDefinedOutputErrorWhenAggregateFails) {
  const std::string fake_plugin =
      "{name:\"nighthawk.fake_user_defined_output\",typed_config:"
      "{\"@type\":\"type.googleapis.com/nighthawk.FakeUserDefinedOutputConfig\","
      "fail_per_worker_output:true}}";
  const std::string logging_plugin =
      "{name:\"nighthawk.log_response_headers_plugin\",typed_config:"
      "{\"@type\":\"type.googleapis.com/nighthawk.LogResponseHeadersConfig\","
      "logging_mode:\"LM_LOG_ALL_RESPONSES\"}}";
  options_ =
      TestUtility::createOptionsImpl(fmt::format("foo --concurrency 1 --user-defined-plugin-config "
                                                 "{} --user-defined-plugin-config {} https://{}/",
                                                 fake_plugin, logging_plugin, loopback_address_));

  // Expect connection failure, but ensure that User Defined Outputs were set up correctly.
  EXPECT_TRUE(runProcess(RunExpectation::EXPECT_FAILURE).ok());

  nighthawk::client::UserDefinedOutput expected_logging_output;
  std::ignore = TextFormat::ParseFromString(R"(plugin_name: "nighthawk.log_response_headers_plugin"
                                 typed_output {
                                   [type.googleapis.com/nighthawk.LogResponseHeadersOutput] {}
                                 }
                                 )",
                                            &expected_logging_output);

  ASSERT_EQ(output_proto_.results_size(), 1);
  const nighthawk::client::Result& result = output_proto_.results(0);
  ASSERT_EQ(result.user_defined_outputs_size(), 2);
  const nighthawk::client::UserDefinedOutput actual_fake_output =
      result.user_defined_outputs(0).plugin_name() == "nighthawk.fake_user_defined_output"
          ? result.user_defined_outputs(0)
          : result.user_defined_outputs(1);
  const nighthawk::client::UserDefinedOutput actual_logging_output =
      result.user_defined_outputs(0).plugin_name() == "nighthawk.log_response_headers_plugin"
          ? result.user_defined_outputs(0)
          : result.user_defined_outputs(1);
  EXPECT_THAT(actual_logging_output, EqualsProto(expected_logging_output));
  EXPECT_FALSE(actual_fake_output.has_typed_output());
  EXPECT_THAT(actual_fake_output.error_message(),
              HasSubstr("Cannot aggregate if any per_worker_outputs failed"));
}

TEST_P(ProcessTest, FailsIfAnyUserDefinedOutputPluginsFailToCreate) {
  const std::string valid_plugin =
      "{name:\"nighthawk.fake_user_defined_output\",typed_config:"
      "{\"@type\":\"type.googleapis.com/nighthawk.FakeUserDefinedOutputConfig\"}}";
  const std::string invalid_logging_plugin =
      "{name:\"nighthawk.fake_user_defined_output\",typed_config:"
      "{\"@type\":\"type.googleapis.com/nighthawk.LogResponseHeadersConfig\"}}";
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --concurrency 1 --user-defined-plugin-config "
                  "{} --user-defined-plugin-config {} https://{}/",
                  valid_plugin, invalid_logging_plugin, loopback_address_));

  EXPECT_TRUE(runProcess(RunExpectation::EXPECT_FAILURE).ok());
  // We always expect failure, so also ensure that actually no work was done.
  EXPECT_EQ(output_proto_.results_size(), 0);
}

TEST_P(ProcessTest, CreatesNoUserDefinedOutputPluginsIfNoConfigs) {
  options_ = TestUtility::createOptionsImpl(fmt::format("foo https://{}/", loopback_address_));

  EXPECT_TRUE(runProcess(RunExpectation::EXPECT_FAILURE).ok());
  EXPECT_EQ(output_proto_.results(0).user_defined_outputs_size(), 0);
}

TEST_P(ProcessTest, FailsWhenDnsResolverFactoryFails) {
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --duration 300 --failure-predicate foo:0 --concurrency 2 https://{}/",
                  loopback_address_));
  EXPECT_TRUE(runProcess(RunExpectation::EXPECT_FAILURE, /*do_cancel=*/false,
                         /*terminate_right_away=*/false, /*bad_dns_resolver_factory=*/true)
                  .ok());
}

TEST_P(ProcessTest, TestWithEncapsulation) {
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --tunnel-uri https://{}/ --tunnel-protocol http1 --protocol http1 "
                  "--concurrency 2 https://{}/",
                  loopback_address_, loopback_address_));
  EXPECT_TRUE(runProcess(RunExpectation::EXPECT_FAILURE).ok());
}

// Regression test: ProcessImpl::shutdown() used to serialize per-worker drains, making
// shutdown take ~concurrency x timeout instead of ~1 x timeout. worker->shutdown() fired
// signal_thread_to_exit_ AND joined the thread for one worker before moving on to the next,
// and a signaled worker thread runs ClientWorkerImpl::shutdownThread() ->
// BenchmarkClientHttpImpl::terminate(), which blocks for up to --timeout while waiting for
// the connection pool to drain. The wedged server below never responds, so the pool never
// goes idle and every worker's drain hits the full timeout; shutdown() now signals all
// workers before joining any, so the drains overlap.
TEST_P(ProcessTest, ShutdownDrainsWorkersConcurrently) {
  constexpr int kConcurrency = 3;
  constexpr int kDrainTimeoutSeconds = 2;
  WedgedTcpServer wedged_server(GetParam());
  // The failure predicate wipes the stock ones, so that execution terminates on --duration only
  // and requests are guaranteed to still be in flight when shutdown starts. Note the plain http
  // scheme: TCP connects to the wedged server succeed immediately (--timeout doubles as the
  // connect timeout), requests are then written and never answered.
  options_ = TestUtility::createOptionsImpl(
      fmt::format("foo --duration 1 --rps 5 --timeout {} --concurrency {} --failure-predicate "
                  "foo:0 -v error http://{}:{}/",
                  kDrainTimeoutSeconds, kConcurrency, loopback_address_, wedged_server.port()));

  TypedExtensionConfig typed_dns_resolver_config;
  Envoy::Network::DnsResolverFactory& dns_resolver_factory =
      Envoy::Network::createDefaultDnsResolverFactory(typed_dns_resolver_config);
  absl::StatusOr<ProcessPtr> process_or_status = ProcessImpl::CreateProcessImpl(
      *options_, dns_resolver_factory, std::move(typed_dns_resolver_config), time_system_);
  ASSERT_TRUE(process_or_status.ok());
  ProcessPtr process = std::move(process_or_status.value());
  OutputCollectorImpl collector(time_system_, *options_);
  EXPECT_TRUE(process->run(collector));
  output_proto_ = collector.toProto();

  // Sanity check: every worker issued at least one request against the wedged server. The server
  // never responds, so all issued requests are still in flight at shutdown, forcing each worker's
  // connection pool drain to run into the full --timeout.
  int workers_with_in_flight_requests = 0;
  for (const auto& result : output_proto_.results()) {
    if (result.name() == "global") {
      continue;
    }
    for (const auto& counter : result.counters()) {
      if (counter.name() == "upstream_rq_total" && counter.value() > 0) {
        workers_with_in_flight_requests++;
      }
    }
  }
  EXPECT_EQ(workers_with_in_flight_requests, kConcurrency);

  const Envoy::MonotonicTime shutdown_start = time_system_.monotonicTime();
  process->shutdown();
  const std::chrono::duration<double> shutdown_duration =
      time_system_.monotonicTime() - shutdown_start;

  // Lower bound: at least one full pool drain timeout must elapse. This proves requests were
  // actually in flight during shutdown and guards against this test passing vacuously.
  EXPECT_GE(shutdown_duration.count(), 0.75 * kDrainTimeoutSeconds);
  // Upper bound: the per-worker drains should overlap, so shutdown should take ~1 x timeout
  // regardless of concurrency, while a serialized implementation takes ~concurrency x
  // timeout (~6s here). The 2 x timeout (4s) bound leaves ~2s of headroom over the
  // timer-dominated concurrent drain for sanitizer-slowed teardown on loaded CI machines,
  // while still failing the serialized case by a full drain-timeout of margin.
  EXPECT_LT(shutdown_duration.count(), 2.0 * kDrainTimeoutSeconds);
}

/**
 * Fixture for executing the Nighthawk process with simulated time.
 */
class ProcessTestWithSimTime : public Envoy::Event::TestUsingSimulatedTime,
                               public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  ProcessTestWithSimTime()
      : options_(TestUtility::createOptionsImpl(
            fmt::format("foo --duration 1 -v error --failure-predicate foo:0 --rps 10 https://{}/",
                        Envoy::Network::Test::getLoopbackAddressUrlString(GetParam())))) {};

protected:
  absl::Status run(std::function<void(bool, const nighthawk::client::Output&)> verify_callback) {
    absl::Status process_status;

    auto run_thread = std::thread([this, &verify_callback, &process_status] {
      TypedExtensionConfig typed_dns_resolver_config;
      Envoy::Network::DnsResolverFactory& dns_resolver_factory =
          Envoy::Network::createDefaultDnsResolverFactory(typed_dns_resolver_config);
      absl::StatusOr<ProcessPtr> process_or_status = ProcessImpl::CreateProcessImpl(
          *options_, dns_resolver_factory, std::move(typed_dns_resolver_config), simTime());
      if (!process_or_status.ok()) {
        process_status = process_or_status.status();
        return;
      }

      ProcessPtr process = std::move(process_or_status.value());
      OutputCollectorImpl collector(simTime(), *options_);
      const bool result = process->run(collector);
      process->shutdown();
      verify_callback(result, collector.toProto());
    });

    // We introduce real-world sleeps to give the executing ProcessImpl
    // an opportunity to observe passage of simulated time. We increase simulated
    // time in three steps, to give it an opportunity to start at the wrong time
    // in case there is an error in the scheduling logic it implements.
    // Note that these sleeps may seem excessively long, but sanitizer runs may need that.
    sleep(1);
    // Move time to 1 second before the scheduled execution time.
    simTime().setSystemTime(options_->scheduled_start().value() - 1s);
    sleep(1);
    // Move time right up to the scheduled execution time.
    simTime().setSystemTime(options_->scheduled_start().value());
    sleep(1);
    // Move time past the scheduled execution time and execution duration.
    simTime().setSystemTime(options_->scheduled_start().value() + 2s);
    // Wait for execution to wrap up.
    run_thread.join();

    return process_status;
  }

  void setScheduleOnOptions(std::chrono::nanoseconds ns_since_epoch) {
    CommandLineOptionsPtr command_line = options_->toCommandLineOptions();
    *(command_line->mutable_scheduled_start()) =
        Envoy::Protobuf::util::TimeUtil::NanosecondsToTimestamp(ns_since_epoch.count());
    options_ = std::make_unique<OptionsImpl>(*command_line);
  }

  OptionsPtr options_;
};

INSTANTIATE_TEST_SUITE_P(IpVersions, ProcessTestWithSimTime,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

// Verify that scheduling execution ahead of time works, and that the execution start timestamp
// associated to the worker result correctly reflects the scheduled time. This should be spot on
// because we use simulated time.
TEST_P(ProcessTestWithSimTime, ScheduleAheadWorks) {
  for (const auto& relative_schedule : std::vector<std::chrono::nanoseconds>{30s, 1h}) {
    setScheduleOnOptions(
        std::chrono::nanoseconds(simTime().systemTime().time_since_epoch() + relative_schedule));
    EXPECT_TRUE(run([this](bool success, const nighthawk::client::Output& output) {
                  EXPECT_TRUE(success);
                  ASSERT_EQ(output.results_size(), 1);
                  EXPECT_EQ(Envoy::ProtobufUtil::TimeUtil::TimestampToNanoseconds(
                                output.results()[0].execution_start()),
                            std::chrono::duration_cast<std::chrono::nanoseconds>(
                                options_->scheduled_start().value().time_since_epoch())
                                .count());
                }).ok());
  }
}

// Verify that scheduling an execution in the past yields an error.
TEST_P(ProcessTestWithSimTime, ScheduleInThePastFails) {
  setScheduleOnOptions(std::chrono::nanoseconds(simTime().systemTime().time_since_epoch() - 1s));
  EXPECT_TRUE(run([](bool success, const nighthawk::client::Output& output) {
                EXPECT_FALSE(success);
                EXPECT_EQ(output.results_size(), 0);
              }).ok());
}

} // namespace
} // namespace Client
} // namespace Nighthawk
