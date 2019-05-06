#include <chrono>

#include "envoy/thread_local/thread_local.h"

#include "common/api/api_impl.h"
#include "common/common/compiler_requirements.h"
#include "common/common/thread_impl.h"
#include "common/event/dispatcher_impl.h"
#include "common/filesystem/filesystem_impl.h"
#include "common/http/header_map_impl.h"
#include "common/network/dns_impl.h"
#include "common/network/utility.h"
#include "common/platform_util_impl.h"
#include "common/rate_limiter_impl.h"
#include "common/runtime/runtime_impl.h"
#include "common/sequencer_impl.h"
#include "common/statistic_impl.h"
#include "common/stats/isolated_store_impl.h"
#include "common/thread_local/thread_local_impl.h"
#include "common/uri_impl.h"
#include "common/utility.h"

#include "client/benchmark_client_impl.h"

#include "test/integration/http_integration.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"
#include "test/mocks/runtime/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/server/utility.h"
#include "test/test_common/utility.h"

#include "ares.h"
#include "gtest/gtest.h"

using namespace std::chrono_literals;
using namespace testing;

namespace Nighthawk {

class BenchmarkClientTestBase : public Envoy::BaseIntegrationTest,
                                public TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  BenchmarkClientTestBase()
      : Envoy::BaseIntegrationTest(GetParam(), realTime(), BenchmarkClientTestBase::envoy_config),
        api_(thread_factory_, store_, timeSystem(), file_system_),
        dispatcher_(api_.allocateDispatcher()) {}

  static void SetUpTestCase() {
    Envoy::Filesystem::InstanceImplPosix file_system;
    envoy_config = file_system.fileReadToEnd(Envoy::TestEnvironment::runfilesPath(
        "test/test_data/benchmark_http_client_test_envoy.yaml"));
    envoy_config = Envoy::TestEnvironment::substitute(envoy_config);
  }

  void SetUp() override {
    ares_library_init(ARES_LIB_INIT_ALL);
    Envoy::Event::Libevent::Global::initialize();
  }

  void TearDown() override {
    if (client_ != nullptr) {
      client_->terminate();
    }
    test_server_.reset();
    fake_upstreams_.clear();
    tls_.shutdownGlobalThreading();
    ares_library_cleanup();
  }

  uint32_t getTestServerHostAndPort() { return lookupPort("listener_0"); }

  void testBasicFunctionality(absl::string_view uriPath, const uint64_t max_pending,
                              const uint64_t connection_limit, const bool use_h2,
                              const uint64_t amount_of_request) {
    setupBenchmarkClient(uriPath, use_h2, false);

    client_->setConnectionTimeout(10s);
    client_->setMaxPendingRequests(max_pending);
    client_->setConnectionLimit(connection_limit);
    client_->initialize(runtime_);

    const uint64_t amount = amount_of_request;
    uint64_t inflight_response_count = 0;

    std::function<void()> f = [this, &inflight_response_count]() {
      --inflight_response_count;
      if (inflight_response_count == 0) {
        dispatcher_->exit();
      }
    };

    for (uint64_t i = 0; i < amount; i++) {
      if (client_->tryStartOne(f)) {
        inflight_response_count++;
      }
    }

    EXPECT_EQ(max_pending, inflight_response_count);
    dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
    EXPECT_EQ(0, getCounter("benchmark.stream_resets"));
  }

  virtual void setupBenchmarkClient(absl::string_view uriPath, bool use_h2,
                                    bool prefetch_connections) = 0;

  void doSetupBenchmarkClient(absl::string_view uriPath, bool use_https, bool use_h2,
                              bool prefetch_connections) {
    const std::string address = Envoy::Network::Test::getLoopbackAddressUrlString(GetParam());
    auto uri = std::make_unique<UriImpl>(fmt::format("{}://{}:{}{}", use_https ? "https" : "http",
                                                     address, getTestServerHostAndPort(), uriPath));
    uri->resolve(*dispatcher_, GetParam() == Envoy::Network::Address::IpVersion::v4
                                   ? Envoy::Network::DnsLookupFamily::V4Only
                                   : Envoy::Network::DnsLookupFamily::V6Only);
    client_ = std::make_unique<Client::BenchmarkClientHttpImpl>(
        api_, *dispatcher_, store_, std::make_unique<StreamingStatistic>(),
        std::make_unique<StreamingStatistic>(), std::move(uri), use_h2, prefetch_connections);
  }

  uint64_t nonZeroValuedCounterCount() {
    return Utility()
        .mapCountersFromStore(client_->store(),
                              [](absl::string_view, uint64_t value) { return value > 0; })
        .size();
  }

  uint64_t getCounter(absl::string_view name) {
    return client_->store().counter("client." + std::string(name)).value();
  }

  Envoy::Thread::ThreadFactoryImplPosix thread_factory_;
  Envoy::Stats::IsolatedStoreImpl store_;
  Envoy::Api::Impl api_;
  Envoy::Event::DispatcherPtr dispatcher_;
  Envoy::Runtime::RandomGeneratorImpl generator_;
  NiceMock<Envoy::ThreadLocal::MockInstance> tls_;
  NiceMock<Envoy::Runtime::MockLoader> runtime_;
  std::unique_ptr<Client::BenchmarkClientHttpImpl> client_;
  Envoy::Filesystem::InstanceImplPosix file_system_;
  static std::string envoy_config;
};

class BenchmarkClientHttpTest : public BenchmarkClientTestBase {
public:
  void SetUp() override { BenchmarkClientHttpTest::initialize(); }

  void setupBenchmarkClient(absl::string_view uriPath, bool use_h2,
                            bool prefetch_connections) override {
    doSetupBenchmarkClient(uriPath, false, use_h2, prefetch_connections);
  };
};

class BenchmarkClientHttpsTest : public BenchmarkClientTestBase {
public:
  void SetUp() override { BenchmarkClientHttpsTest::initialize(); }

  void setupBenchmarkClient(absl::string_view uriPath, bool use_h2,
                            bool prefetch_connections) override {
    doSetupBenchmarkClient(uriPath, true, use_h2, prefetch_connections);
  };

  void initialize() override {
    config_helper_.addConfigModifier([](envoy::config::bootstrap::v2::Bootstrap& bootstrap) {
      auto* common_tls_context = bootstrap.mutable_static_resources()
                                     ->mutable_listeners(0)
                                     ->mutable_filter_chains(0)
                                     ->mutable_tls_context()
                                     ->mutable_common_tls_context();
      common_tls_context->mutable_validation_context_sds_secret_config()->set_name(
          "validation_context");
      common_tls_context->add_tls_certificate_sds_secret_configs()->set_name("server_cert");

      auto* secret = bootstrap.mutable_static_resources()->add_secrets();
      secret->set_name("validation_context");
      auto* validation_context = secret->mutable_validation_context();
      validation_context->mutable_trusted_ca()->set_filename(Envoy::TestEnvironment::runfilesPath(
          "external/envoy/test/config/integration/certs/cacert.pem"));
      secret = bootstrap.mutable_static_resources()->add_secrets();
      secret->set_name("server_cert");
      auto* tls_certificate = secret->mutable_tls_certificate();
      tls_certificate->mutable_certificate_chain()->set_filename(
          Envoy::TestEnvironment::runfilesPath(
              "external/envoy/test/config/integration/certs/servercert.pem"));
      tls_certificate->mutable_private_key()->set_filename(Envoy::TestEnvironment::runfilesPath(
          "external/envoy/test/config/integration/certs/serverkey.pem"));
    });

    BenchmarkClientTestBase::initialize();
  }
};

std::string BenchmarkClientTestBase::envoy_config;

INSTANTIATE_TEST_SUITE_P(IpVersions, BenchmarkClientHttpTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);
INSTANTIATE_TEST_SUITE_P(IpVersions, BenchmarkClientHttpsTest,
                         ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()),
                         Envoy::TestUtility::ipTestParamsToString);

TEST_P(BenchmarkClientHttpTest, BasicTestH1) {
  testBasicFunctionality("/lorem-ipsum-status-200", 1, 1, false, 10);

  EXPECT_EQ(1, getCounter("upstream_cx_http1_total"));
  EXPECT_LE(3621, getCounter("upstream_cx_rx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_LE(78, getCounter("upstream_cx_tx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_total"));
  EXPECT_EQ(1, getCounter("benchmark.http_2xx"));
  EXPECT_EQ(7, nonZeroValuedCounterCount());
}

TEST_P(BenchmarkClientHttpTest, BasicTestH1404) {
  testBasicFunctionality("/lorem-ipsum-status-404", 1, 1, false, 10);

  EXPECT_EQ(1, getCounter("upstream_cx_http1_total"));
  EXPECT_EQ(0, getCounter("upstream_cx_protocol_error"));
  EXPECT_LE(97, getCounter("upstream_cx_rx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_LE(78, getCounter("upstream_cx_tx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_total"));
  EXPECT_EQ(1, getCounter("benchmark.http_4xx"));
  EXPECT_EQ(7, nonZeroValuedCounterCount());
}

TEST_P(BenchmarkClientHttpsTest, BasicTestHttpsH1) {
  testBasicFunctionality("/lorem-ipsum-status-200", 1, 1, false, 10);

  EXPECT_EQ(1, getCounter("ssl.ciphers.ECDHE-RSA-AES128-GCM-SHA256"));
  EXPECT_EQ(1, getCounter("ssl.curves.X25519"));
  EXPECT_EQ(1, getCounter("ssl.handshake"));
  EXPECT_EQ(1, getCounter("ssl.sigalgs.rsa_pss_rsae_sha256"));
  EXPECT_EQ(1, getCounter("ssl.versions.TLSv1.2"));
  EXPECT_EQ(1, getCounter("upstream_cx_http1_total"));
  EXPECT_LE(3622, getCounter("upstream_cx_rx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_LE(78, getCounter("upstream_cx_tx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_total"));
  EXPECT_EQ(1, getCounter("benchmark.http_2xx"));
  EXPECT_EQ(12, nonZeroValuedCounterCount());
}

TEST_P(BenchmarkClientHttpsTest, BasicTestH2) {
  testBasicFunctionality("/lorem-ipsum-status-200", 1, 1, true, 10);

  EXPECT_EQ(1, getCounter("ssl.ciphers.ECDHE-RSA-AES128-GCM-SHA256"));
  EXPECT_EQ(1, getCounter("ssl.curves.X25519"));
  EXPECT_EQ(1, getCounter("ssl.handshake"));
  EXPECT_EQ(1, getCounter("ssl.sigalgs.rsa_pss_rsae_sha256"));
  EXPECT_EQ(1, getCounter("ssl.versions.TLSv1.2"));
  EXPECT_EQ(1, getCounter("upstream_cx_http2_total"));
  EXPECT_LE(3585, getCounter("upstream_cx_rx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_LE(108, getCounter("upstream_cx_tx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_total"));
  EXPECT_EQ(1, getCounter("benchmark.http_2xx"));
  EXPECT_EQ(12, nonZeroValuedCounterCount());
}

TEST_P(BenchmarkClientHttpTest, BasicTestH2C) {
  testBasicFunctionality("/lorem-ipsum-status-200", 1, 1, true, 10);

  EXPECT_EQ(1, getCounter("upstream_cx_http2_total"));
  EXPECT_LE(3584, getCounter("upstream_cx_rx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_LE(108, getCounter("upstream_cx_tx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_total"));
  EXPECT_EQ(1, getCounter("benchmark.http_2xx"));
  EXPECT_EQ(7, nonZeroValuedCounterCount());
}

// TODO(oschaaf): can't configure envoy to emit a weird status, fix
// this later.
TEST_P(BenchmarkClientHttpTest, DISABLED_WeirdStatus) {
  testBasicFunctionality("/601", 1, 1, false, 10);

  EXPECT_EQ(1, getCounter("upstream_cx_http1_total"));
  EXPECT_LE(3621, getCounter("upstream_cx_rx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_LE(78, getCounter("upstream_cx_tx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_total"));
  EXPECT_EQ(1, getCounter("benchmark.http_xxx"));
  EXPECT_EQ(7, nonZeroValuedCounterCount());
}

TEST_P(BenchmarkClientHttpTest, H1ConnectionFailure) {
  // Kill the test server, so we can't connect.
  // We allow a single connection and no pending. We expect one connection failure.
  test_server_.reset();
  testBasicFunctionality("/lorem-ipsum-status-200", 1, 1, false, 10);

  EXPECT_EQ(1, getCounter("upstream_cx_connect_fail"));
  EXPECT_LE(1, getCounter("upstream_cx_http1_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_LE(1, getCounter("upstream_rq_pending_failure_eject"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(5, nonZeroValuedCounterCount());
}

TEST_P(BenchmarkClientHttpTest, H1MultiConnectionFailure) {
  // Kill the test server, so we can't connect.
  // We allow ten connections and ten pending requests. We expect ten connection failures.
  test_server_.reset();
  testBasicFunctionality("/lorem-ipsum-status-200", 10, 10, false, 10);

  EXPECT_EQ(10, getCounter("upstream_cx_connect_fail"));
  EXPECT_LE(10, getCounter("upstream_cx_http1_total"));
  EXPECT_EQ(10, getCounter("upstream_cx_total"));
  EXPECT_LE(10, getCounter("upstream_rq_pending_failure_eject"));
  EXPECT_EQ(10, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(5, nonZeroValuedCounterCount());
}

TEST_P(BenchmarkClientHttpTest, EnableLatencyMeasurement) {
  setupBenchmarkClient("/", false, false);
  int callback_count = 0;
  client_->initialize(runtime_);

  EXPECT_EQ(false, client_->measureLatencies());
  EXPECT_EQ(true, client_->tryStartOne([&]() {
    callback_count++;
    dispatcher_->exit();
  }));
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(0, client_->statistics()["benchmark_http_client.queue_to_connect"]->count());
  EXPECT_EQ(0, client_->statistics()["benchmark_http_client.request_to_response"]->count());
  client_->setMeasureLatencies(true);
  EXPECT_EQ(true, client_->measureLatencies());
  EXPECT_EQ(true, client_->tryStartOne([&]() {
    callback_count++;
    dispatcher_->exit();
  }));

  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);

  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(1, client_->statistics()["benchmark_http_client.queue_to_connect"]->count());
  EXPECT_EQ(1, client_->statistics()["benchmark_http_client.request_to_response"]->count());
}

TEST_P(BenchmarkClientHttpTest, StatusTrackingInOnComplete) {
  auto uri = std::make_unique<UriImpl>("http://foo/");
  auto store = std::make_unique<Envoy::Stats::IsolatedStoreImpl>();
  client_ = std::make_unique<Client::BenchmarkClientHttpImpl>(
      api_, *dispatcher_, *store, std::make_unique<StreamingStatistic>(),
      std::make_unique<StreamingStatistic>(), std::move(uri), false, false);
  Envoy::Http::HeaderMapImpl header;

  auto& status = header.insertStatus();

  status.value(1);
  client_->onComplete(true, header);
  status.value(100);
  client_->onComplete(true, header);
  status.value(200);
  client_->onComplete(true, header);
  status.value(300);
  client_->onComplete(true, header);
  status.value(400);
  client_->onComplete(true, header);
  status.value(500);
  client_->onComplete(true, header);
  status.value(600);
  client_->onComplete(true, header);
  status.value(200);
  // Shouldn't be counted by status, should add to stream reset.
  client_->onComplete(false, header);

  EXPECT_EQ(1, getCounter("benchmark.http_2xx"));
  EXPECT_EQ(1, getCounter("benchmark.http_3xx"));
  EXPECT_EQ(1, getCounter("benchmark.http_4xx"));
  EXPECT_EQ(1, getCounter("benchmark.http_5xx"));
  EXPECT_EQ(2, getCounter("benchmark.http_xxx"));
  EXPECT_EQ(1, getCounter("benchmark.stream_resets"));

  client_.reset();
}

TEST_P(BenchmarkClientHttpTest, ConnectionPrefetching) {
  setupBenchmarkClient("/", false, true);
  client_->setConnectionLimit(50);
  client_->initialize(runtime_);
  EXPECT_EQ(true, client_->tryStartOne([&]() { dispatcher_->exit(); }));
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);
  EXPECT_EQ(50, getCounter("upstream_cx_total"));
}

TEST_P(BenchmarkClientHttpTest, RequestMethodPost) {
  setupBenchmarkClient("/", false, true);
  EXPECT_EQ("GET", client_->requestHeaders().Method()->value().getStringView());
  client_->setRequestMethod("POST");
  client_->setRequestHeader("a", "b");
  client_->setRequestHeader("c", "d");
  client_->setRequestBodySize(1313);

  EXPECT_EQ("POST", client_->requestHeaders().Method()->value().getStringView());
  EXPECT_EQ(
      "b",
      client_->requestHeaders().get(Envoy::Http::LowerCaseString("a"))->value().getStringView());
  EXPECT_EQ(
      "d",
      client_->requestHeaders().get(Envoy::Http::LowerCaseString("c"))->value().getStringView());

  client_->initialize(runtime_);

  EXPECT_EQ(true, client_->tryStartOne([&]() { dispatcher_->exit(); }));
  dispatcher_->run(Envoy::Event::Dispatcher::RunType::Block);

  EXPECT_EQ(1, getCounter("benchmark.http_4xx"));
  EXPECT_EQ(GetParam() == Envoy::Network::Address::IpVersion::v4 ? 1407 : 1403,
            getCounter("upstream_cx_tx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_overflow"));
  EXPECT_EQ(116, getCounter("upstream_cx_rx_bytes_total"));
  EXPECT_EQ(1, getCounter("upstream_rq_pending_total"));
  EXPECT_EQ(1, getCounter("upstream_cx_close_notify"));
  EXPECT_EQ(1, getCounter("upstream_cx_http1_total"));
  EXPECT_EQ(9, nonZeroValuedCounterCount());
}

// TODO(oschaaf): test protocol violations, stream resets, etc.

} // namespace Nighthawk
