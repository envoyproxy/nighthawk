#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "envoy/config/metrics/v3/stats.pb.h"

#include "external/envoy/source/common/config/utility.h"
#include "external/envoy/source/common/network/address_impl.h"
#include "external/envoy/source/common/stats/isolated_store_impl.h"
#include "external/envoy/test/mocks/stats/mocks.h"
#include "external/envoy/test/mocks/thread_local/mocks.h"

#include "source/common/statistic_impl.h"
#include "source/common/statsd_sink.h"

#include "absl/strings/str_split.h"
#include "gtest/gtest.h"

using namespace testing;

namespace Nighthawk {

// Minimal loopback UDP receiver.
class UdpReceiver {
public:
  UdpReceiver() {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    EXPECT_GE(fd_, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    EXPECT_EQ(0, ::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));
    socklen_t len = sizeof(addr);
    EXPECT_EQ(0, ::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &len));
    port_ = ntohs(addr.sin_port);
  }
  ~UdpReceiver() { ::close(fd_); }

  uint32_t port() const { return port_; }
  Envoy::Network::Address::InstanceConstSharedPtr address() const {
    return std::make_shared<Envoy::Network::Address::Ipv4Instance>("127.0.0.1", port_);
  }

  // Returns the next datagram, or an empty string after a 2s timeout.
  std::string receive() {
    pollfd pfd{fd_, POLLIN, 0};
    if (::poll(&pfd, 1, 2000) <= 0) {
      return "";
    }
    char buffer[65536];
    const ssize_t n = ::recv(fd_, buffer, sizeof(buffer), 0);
    return n > 0 ? std::string(buffer, n) : "";
  }

private:
  int fd_{-1};
  uint32_t port_{0};
};

class StatsdSinkTest : public Test {
public:
  std::unique_ptr<StatsdSink> makeSink(bool use_tags, std::optional<uint64_t> max_bytes = {},
                                       absl::string_view prefix = "",
                                       std::vector<std::string> extra_tags = {}) {
    return std::make_unique<StatsdSink>(receiver_.address(), prefix, use_tags, max_bytes,
                                        extra_tags);
  }

  UdpReceiver receiver_;
  NiceMock<Envoy::ThreadLocal::MockInstance> tls_;
  Envoy::Stats::IsolatedStoreImpl store_;
};

TEST_F(StatsdSinkTest, MessageNamingWithoutTags) {
  auto sink = makeSink(false);
  EXPECT_EQ("nighthawk", sink->prefix());
  // Both "cluster.<n>." and "worker.<n>." store prefixes are normalized to "worker.<n>.".
  EXPECT_EQ("nighthawk.worker.0.benchmark.http_2xx:5|c",
            sink->buildMessage("cluster.0.benchmark.http_2xx", "5", "c", std::nullopt));
  EXPECT_EQ("nighthawk.worker.2.sequencer.failed_terminations:1|c",
            sink->buildMessage("worker.2.sequencer.failed_terminations", "1", "c", std::nullopt));
  EXPECT_EQ("nighthawk.worker.3.benchmark_http_client.latency_2xx:1.500|ms",
            sink->buildMessage("benchmark_http_client.latency_2xx", "1.500", "ms", 3));
  EXPECT_EQ("nighthawk.server.total_connections:7|g",
            sink->buildMessage("server.total_connections", "7", "g", std::nullopt));
}

TEST_F(StatsdSinkTest, MessageNamingWithTagsAndPrefix) {
  auto sink = makeSink(true, {}, "lg");
  EXPECT_EQ("lg.benchmark.http_2xx:5|c|#worker:0",
            sink->buildMessage("cluster.0.benchmark.http_2xx", "5", "c", std::nullopt));
  EXPECT_EQ("lg.benchmark_stream.message_latency:0.004|ms|#worker:1",
            sink->buildMessage("benchmark_stream.message_latency", "0.004", "ms", 1));
  EXPECT_EQ("lg.server.total_connections:7|g",
            sink->buildMessage("server.total_connections", "7", "g", std::nullopt));
}

TEST_F(StatsdSinkTest, ExtraTagsAreAppendedWithTagsAndIgnoredWithout) {
  auto tagged = makeSink(true, {}, "", {"run:phase-c", "pod:driver-1"});
  EXPECT_EQ("nighthawk.benchmark.http_2xx:5|c|#worker:0,run:phase-c,pod:driver-1",
            tagged->buildMessage("cluster.0.benchmark.http_2xx", "5", "c", std::nullopt));
  EXPECT_EQ("nighthawk.server.total_connections:7|g|#run:phase-c,pod:driver-1",
            tagged->buildMessage("server.total_connections", "7", "g", std::nullopt));
  auto plain = makeSink(false, {}, "", {"run:phase-c"});
  EXPECT_EQ("nighthawk.worker.0.benchmark.http_2xx:5|c",
            plain->buildMessage("cluster.0.benchmark.http_2xx", "5", "c", std::nullopt));
}

TEST_F(StatsdSinkTest, FormatsNanosecondsAsMilliseconds) {
  EXPECT_EQ("1.500", StatsdSink::formatNanosAsMillis(1500000));
  EXPECT_EQ("0.001", StatsdSink::formatNanosAsMillis(1000));
  EXPECT_EQ("1234.568", StatsdSink::formatNanosAsMillis(1234567890));
  EXPECT_EQ("0.000", StatsdSink::formatNanosAsMillis(0));
}

TEST_F(StatsdSinkTest, HistogramValuesAreSentAsMillisecondTimings) {
  auto sink = makeSink(false);
  SinkableHdrStatistic statistic(*store_.rootScope(), /*worker_id=*/3);
  statistic.setId("benchmark_http_client.latency_2xx");
  sink->onHistogramComplete(statistic, 2500000); // 2.5 ms in nanoseconds
  EXPECT_EQ("nighthawk.worker.3.benchmark_http_client.latency_2xx:2.500|ms", receiver_.receive());

  auto tagged = makeSink(true);
  tagged->onHistogramComplete(statistic, 750000);
  EXPECT_EQ("nighthawk.benchmark_http_client.latency_2xx:0.750|ms|#worker:3", receiver_.receive());
}

TEST_F(StatsdSinkTest, HistogramSamplesAreBatchedAndFlushedOnDestruction) {
  SinkableHdrStatistic statistic(*store_.rootScope(), /*worker_id=*/0);
  statistic.setId("benchmark_stream.message_latency");
  {
    auto sink = makeSink(true, /*max_bytes=*/130);
    for (uint64_t i = 1; i <= 3; i++) {
      sink->onHistogramComplete(statistic, i * 1000000); // 1, 2, 3 ms
    }
    // Three 61-byte messages: the first two fit one 130-byte datagram, the third starts a new
    // batch that is still pending.
    EXPECT_EQ(
        std::vector<std::string>({"nighthawk.benchmark_stream.message_latency:1.000|ms|#worker:0",
                                  "nighthawk.benchmark_stream.message_latency:2.000|ms|#worker:0"}),
        std::vector<std::string>(absl::StrSplit(receiver_.receive(), '\n')));
    // Nothing else arrives until the sink flushes.
    NiceMock<Envoy::Stats::MockMetricSnapshot> snapshot;
    std::vector<Envoy::Stats::MetricSnapshot::CounterSnapshot> counters;
    std::vector<std::reference_wrapper<const Envoy::Stats::Gauge>> gauges;
    EXPECT_CALL(snapshot, counters()).WillRepeatedly(ReturnRef(counters));
    EXPECT_CALL(snapshot, gauges()).WillRepeatedly(ReturnRef(gauges));
    sink->flush(snapshot);
    EXPECT_EQ("nighthawk.benchmark_stream.message_latency:3.000|ms|#worker:0", receiver_.receive());
    sink->onHistogramComplete(statistic, 4000000);
  }
  // Destroying the sink sends the pending batch.
  EXPECT_EQ("nighthawk.benchmark_stream.message_latency:4.000|ms|#worker:0", receiver_.receive());
}

TEST_F(StatsdSinkTest, FlushSendsCounterDeltasAndGauges) {
  auto sink = makeSink(false);
  Envoy::Stats::Counter& counter = store_.counterFromString("cluster.0.benchmark.http_2xx");
  counter.add(5);
  Envoy::Stats::Counter& idle = store_.counterFromString("cluster.0.benchmark.http_5xx");
  Envoy::Stats::Gauge& gauge = store_.gaugeFromString("cluster.0.upstream_cx_active",
                                                      Envoy::Stats::Gauge::ImportMode::Accumulate);
  gauge.set(2);
  NiceMock<Envoy::Stats::MockMetricSnapshot> snapshot;
  std::vector<Envoy::Stats::MetricSnapshot::CounterSnapshot> counters{
      {/*delta=*/5, std::cref(counter)}, {/*delta=*/0, std::cref(idle)}};
  std::vector<std::reference_wrapper<const Envoy::Stats::Gauge>> gauges{std::cref(gauge)};
  EXPECT_CALL(snapshot, counters()).WillRepeatedly(ReturnRef(counters));
  EXPECT_CALL(snapshot, gauges()).WillRepeatedly(ReturnRef(gauges));
  sink->flush(snapshot);
  // Zero deltas are skipped, so exactly two datagrams arrive.
  EXPECT_EQ("nighthawk.worker.0.benchmark.http_2xx:5|c", receiver_.receive());
  EXPECT_EQ("nighthawk.worker.0.upstream_cx_active:2|g", receiver_.receive());
  EXPECT_EQ("", receiver_.receive());
}

TEST_F(StatsdSinkTest, FlushBatchesIntoDatagramsWhenConfigured) {
  auto sink = makeSink(true, /*max_bytes=*/100);
  Envoy::Stats::Counter& a = store_.counterFromString("cluster.0.benchmark.http_2xx");
  Envoy::Stats::Counter& b = store_.counterFromString("cluster.0.benchmark.grpc_error");
  Envoy::Stats::Counter& c = store_.counterFromString("cluster.1.benchmark.http_2xx");
  NiceMock<Envoy::Stats::MockMetricSnapshot> snapshot;
  std::vector<Envoy::Stats::MetricSnapshot::CounterSnapshot> counters{
      {1, std::cref(a)}, {2, std::cref(b)}, {3, std::cref(c)}};
  std::vector<std::reference_wrapper<const Envoy::Stats::Gauge>> gauges;
  EXPECT_CALL(snapshot, counters()).WillRepeatedly(ReturnRef(counters));
  EXPECT_CALL(snapshot, gauges()).WillRepeatedly(ReturnRef(gauges));
  sink->flush(snapshot);
  const std::string first = receiver_.receive();
  const std::string second = receiver_.receive();
  EXPECT_LE(first.size(), 100);
  EXPECT_EQ(std::vector<std::string>({"nighthawk.benchmark.http_2xx:1|c|#worker:0",
                                      "nighthawk.benchmark.grpc_error:2|c|#worker:0"}),
            std::vector<std::string>(absl::StrSplit(first, '\n')));
  EXPECT_EQ("nighthawk.benchmark.http_2xx:3|c|#worker:1", second);
}

TEST_F(StatsdSinkTest, FactoriesAreRegisteredUnderEnvoysSinkNames) {
  auto& statsd = Envoy::Config::Utility::getAndCheckFactoryByName<NighthawkStatsSinkFactory>(
      "envoy.stat_sinks.statsd");
  envoy::config::metrics::v3::StatsdSink config;
  config.mutable_address()->mutable_socket_address()->set_address("127.0.0.1");
  config.mutable_address()->mutable_socket_address()->set_port_value(receiver_.port());
  config.set_prefix("nh");
  std::unique_ptr<Envoy::Stats::Sink> sink =
      statsd.createStatsSink(config, store_.symbolTable(), tls_, {});
  ASSERT_NE(nullptr, sink);
  auto* typed = dynamic_cast<StatsdSink*>(sink.get());
  ASSERT_NE(nullptr, typed);
  EXPECT_EQ("nh", typed->prefix());
  EXPECT_FALSE(typed->useTags());

  envoy::config::metrics::v3::StatsdSink tcp;
  tcp.set_tcp_cluster_name("statsd");
  EXPECT_THROW_WITH_REGEX(statsd.createStatsSink(tcp, store_.symbolTable(), tls_, {}),
                          Envoy::EnvoyException, "only the UDP 'address' form");

  auto& dog = Envoy::Config::Utility::getAndCheckFactoryByName<NighthawkStatsSinkFactory>(
      "envoy.stat_sinks.dog_statsd");
  envoy::config::metrics::v3::DogStatsdSink dog_config;
  dog_config.mutable_address()->mutable_socket_address()->set_address("127.0.0.1");
  dog_config.mutable_address()->mutable_socket_address()->set_port_value(receiver_.port());
  std::unique_ptr<Envoy::Stats::Sink> dog_sink =
      dog.createStatsSink(dog_config, store_.symbolTable(), tls_, {"run:x"});
  auto* dog_typed = dynamic_cast<StatsdSink*>(dog_sink.get());
  ASSERT_NE(nullptr, dog_typed);
  EXPECT_TRUE(dog_typed->useTags());
  EXPECT_EQ("nighthawk", dog_typed->prefix());

  // Host names are resolved at creation time.
  envoy::config::metrics::v3::DogStatsdSink by_name;
  by_name.mutable_address()->mutable_socket_address()->set_address("localhost");
  by_name.mutable_address()->mutable_socket_address()->set_port_value(receiver_.port());
  EXPECT_NE(nullptr, dog.createStatsSink(by_name, store_.symbolTable(), tls_, {}));
  envoy::config::metrics::v3::DogStatsdSink unresolvable;
  unresolvable.mutable_address()->mutable_socket_address()->set_address("no.such.host.invalid");
  unresolvable.mutable_address()->mutable_socket_address()->set_port_value(1);
  EXPECT_THROW_WITH_REGEX(dog.createStatsSink(unresolvable, store_.symbolTable(), tls_, {}),
                          Envoy::EnvoyException, "could not resolve host");

  envoy::config::metrics::v3::DogStatsdSink no_address;
  EXPECT_THROW_WITH_REGEX(dog.createStatsSink(no_address, store_.symbolTable(), tls_, {}),
                          Envoy::EnvoyException, "'address' is required");
}

} // namespace Nighthawk
