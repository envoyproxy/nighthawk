#include <memory>
#include <string>
#include <vector>

#include "envoy/registry/registry.h"

#include "source/common/config/well_known_names.h"
#include "source/common/protobuf/utility.h"
#include "source/common/stats/allocator_impl.h"
#include "source/common/stats/thread_local_store.h"
#include "source/server/configuration_impl.h"
#include "test/mocks/server/server_factory_context.h"
#include "test/mocks/stats/mocks.h"
#include "test/test_common/registry.h"
#include "test/test_common/utility.h"

#include "source/common/statistic_impl.h"
#include "source/stats_sink/envoy_stats_sink_adapter.h"

#include "api/stats_sink/envoy_stats_sink_adapter.pb.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Nighthawk {
namespace {

using ::testing::IsEmpty;
using ::testing::NiceMock;

constexpr char kFakeEnvoySinkName[] = "envoy.stat_sinks.nighthawk_test_fake";

// What an Envoy stats sink observed of one histogram sample. Captured during the call, since the
// histogram passed to onHistogramComplete() is only valid for its duration.
struct Sample {
  const Envoy::Stats::Histogram* histogram;
  std::string name;
  std::string tag_extracted_name;
  Envoy::Stats::TagVector tags;
  Envoy::Stats::Histogram::Unit unit;
  uint64_t value;
};

// What the Envoy stats sink under the adapter observed, shared with the test body.
struct Observed {
  int flushes{0};
  std::vector<Sample> samples;
};

// Stands in for a stats sink implemented as an Envoy extension.
class FakeEnvoySink : public Envoy::Stats::Sink {
public:
  explicit FakeEnvoySink(Observed& observed) : observed_(observed) {}

  void flush(Envoy::Stats::MetricSnapshot&) override { ++observed_.flushes; }
  void onHistogramComplete(const Envoy::Stats::Histogram& histogram, uint64_t value) override {
    observed_.samples.push_back({&histogram, histogram.name(), histogram.tagExtractedName(),
                                 histogram.tags(), histogram.unit(), value});
  }

private:
  Observed& observed_;
};

// Stands in for the factory of a stats sink implemented as an Envoy extension.
class FakeEnvoySinkFactory : public Envoy::Server::Configuration::StatsSinkFactory {
public:
  explicit FakeEnvoySinkFactory(Observed& observed) : observed_(observed) {}

  absl::StatusOr<Envoy::Stats::SinkPtr>
  createStatsSink(const Envoy::Protobuf::Message&,
                  Envoy::Server::Configuration::ServerFactoryContext&) override {
    if (fail_creation_) {
      return absl::InvalidArgumentError("fake sink refused its configuration");
    }
    return std::make_unique<FakeEnvoySink>(observed_);
  }
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<Envoy::Protobuf::Struct>();
  }
  std::string name() const override { return kFakeEnvoySinkName; }

  bool fail_creation_{false};

private:
  Observed& observed_;
};

// Returns the configuration of an adapter that forwards to the Envoy sink with the given name
// and configuration.
envoy::config::metrics::v3::StatsSink
adapterConfigFor(const std::string& envoy_sink_name,
                 const Envoy::Protobuf::Message& envoy_sink_config = Envoy::Protobuf::Struct()) {
  nighthawk::EnvoyStatsSinkAdapterConfig adapter_config;
  adapter_config.mutable_sink()->set_name(envoy_sink_name);
  Envoy::MessageUtil::packFrom(*adapter_config.mutable_sink()->mutable_typed_config(),
                               envoy_sink_config);

  envoy::config::metrics::v3::StatsSink config;
  config.set_name("nighthawk.envoy_stats_sink_adapter");
  Envoy::MessageUtil::packFrom(*config.mutable_typed_config(), adapter_config);
  return config;
}

class EnvoyStatsSinkAdapterTest : public testing::Test {
protected:
  EnvoyStatsSinkAdapterTest() : envoy_factory_(observed_), registered_(envoy_factory_) {}

  // Creates the adapter the way ProcessImpl does: through the registered Nighthawk factory.
  std::unique_ptr<Envoy::Stats::Sink> createAdapter() {
    auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<NighthawkStatsSinkFactory>(
        "nighthawk.envoy_stats_sink_adapter");
    return factory.createStatsSink(adapterConfigFor(kFakeEnvoySinkName), context_);
  }

  Observed observed_;
  FakeEnvoySinkFactory envoy_factory_;
  Envoy::Registry::InjectFactory<Envoy::Server::Configuration::StatsSinkFactory> registered_;
  NiceMock<Envoy::Server::Configuration::MockServerFactoryContext> context_;
  Envoy::Stats::MockIsolatedStatsStore store_;
};

TEST_F(EnvoyStatsSinkAdapterTest, FactoryIsRegisteredUnderItsConfigType) {
  auto& factory = Envoy::Config::Utility::getAndCheckFactory<NighthawkStatsSinkFactory>(
      adapterConfigFor(kFakeEnvoySinkName));
  EXPECT_EQ("nighthawk.envoy_stats_sink_adapter", factory.name());
  Envoy::ProtobufTypes::MessagePtr empty_config = factory.createEmptyConfigProto();
  EXPECT_NE(nullptr, dynamic_cast<nighthawk::EnvoyStatsSinkAdapterConfig*>(empty_config.get()));
}

TEST_F(EnvoyStatsSinkAdapterTest, ForwardsFlushesToTheEnvoySink) {
  std::unique_ptr<Envoy::Stats::Sink> adapter = createAdapter();
  NiceMock<Envoy::Stats::MockMetricSnapshot> snapshot;
  adapter->flush(snapshot);
  adapter->flush(snapshot);
  EXPECT_EQ(2, observed_.flushes);
}

TEST_F(EnvoyStatsSinkAdapterTest, PresentsLatencySamplesAsMicrosecondsOfTheirWorker) {
  std::unique_ptr<Envoy::Stats::Sink> adapter = createAdapter();
  SinkableHdrStatistic statistic(*store_.rootScope(), /*worker_id=*/3);
  statistic.setId("benchmark_http_client.latency_2xx");

  // 1.5 ms, in the nanoseconds Nighthawk records.
  adapter->onHistogramComplete(statistic, 1500000);

  ASSERT_EQ(1, observed_.samples.size());
  const Sample& sample = observed_.samples[0];
  EXPECT_EQ(1500, sample.value);
  EXPECT_EQ(Envoy::Stats::Histogram::Unit::Microseconds, sample.unit);
  EXPECT_EQ("cluster.3.benchmark_http_client.latency_2xx", sample.name);
  EXPECT_EQ("cluster.benchmark_http_client.latency_2xx", sample.tag_extracted_name);
  ASSERT_EQ(1, sample.tags.size());
  EXPECT_EQ(Envoy::Config::TagNames::get().CLUSTER_NAME, sample.tags[0].name_);
  EXPECT_EQ("3", sample.tags[0].value_);
}

TEST_F(EnvoyStatsSinkAdapterTest, PresentsCircllhistLatencySamplesTheSameWay) {
  std::unique_ptr<Envoy::Stats::Sink> adapter = createAdapter();
  SinkableCircllhistStatistic statistic(*store_.rootScope(), /*worker_id=*/0);
  statistic.setId("benchmark_http_client.latency_5xx");

  adapter->onHistogramComplete(statistic, 2000000);

  ASSERT_EQ(1, observed_.samples.size());
  EXPECT_EQ(2000, observed_.samples[0].value);
  EXPECT_EQ(Envoy::Stats::Histogram::Unit::Microseconds, observed_.samples[0].unit);
  EXPECT_EQ("cluster.0.benchmark_http_client.latency_5xx", observed_.samples[0].name);
}

TEST_F(EnvoyStatsSinkAdapterTest, NamesLatencySamplesWithoutAWorkerAfterTheStatisticAlone) {
  std::unique_ptr<Envoy::Stats::Sink> adapter = createAdapter();
  SinkableHdrStatistic statistic(*store_.rootScope());
  statistic.setId("benchmark_http_client.latency_2xx");

  adapter->onHistogramComplete(statistic, 1000);

  ASSERT_EQ(1, observed_.samples.size());
  EXPECT_EQ("benchmark_http_client.latency_2xx", observed_.samples[0].name);
  EXPECT_EQ("benchmark_http_client.latency_2xx", observed_.samples[0].tag_extracted_name);
  EXPECT_THAT(observed_.samples[0].tags, IsEmpty());
}

TEST_F(EnvoyStatsSinkAdapterTest, RoundsLatencySamplesToTheNearestMicrosecond) {
  std::unique_ptr<Envoy::Stats::Sink> adapter = createAdapter();
  SinkableHdrStatistic statistic(*store_.rootScope(), /*worker_id=*/0);
  statistic.setId("benchmark_http_client.latency_2xx");

  adapter->onHistogramComplete(statistic, 40499);
  adapter->onHistogramComplete(statistic, 40500);
  adapter->onHistogramComplete(statistic, 499);

  ASSERT_EQ(3, observed_.samples.size());
  EXPECT_EQ(40, observed_.samples[0].value);
  EXPECT_EQ(41, observed_.samples[1].value);
  EXPECT_EQ(0, observed_.samples[2].value);
}

TEST_F(EnvoyStatsSinkAdapterTest, ForwardsSamplesOfOtherHistogramsUnchanged) {
  std::unique_ptr<Envoy::Stats::Sink> adapter = createAdapter();
  NiceMock<Envoy::Stats::MockHistogram> envoy_histogram;
  envoy_histogram.name_ = "cluster.0.upstream_rq_time";
  envoy_histogram.unit_ = Envoy::Stats::Histogram::Unit::Milliseconds;

  adapter->onHistogramComplete(envoy_histogram, 1500000);

  ASSERT_EQ(1, observed_.samples.size());
  // The very same histogram, not a view of it, and the value as delivered.
  EXPECT_EQ(&envoy_histogram, observed_.samples[0].histogram);
  EXPECT_EQ(1500000, observed_.samples[0].value);
  EXPECT_EQ(Envoy::Stats::Histogram::Unit::Milliseconds, observed_.samples[0].unit);
}

TEST_F(EnvoyStatsSinkAdapterTest, ReceivesSamplesRecordedIntoANighthawkStatistic) {
  // The path a sample takes in a Nighthawk run: the statistic delivers it to the sinks of its
  // store, of which the adapter is one.
  Envoy::Stats::SymbolTableImpl symbol_table;
  Envoy::Stats::AllocatorImpl allocator(symbol_table);
  Envoy::Stats::ThreadLocalStoreImpl store(allocator);
  std::unique_ptr<Envoy::Stats::Sink> adapter = createAdapter();
  store.addSink(*adapter);
  Envoy::Stats::ScopeSharedPtr worker_scope = store.createScope("cluster.0.");
  SinkableHdrStatistic statistic(*worker_scope, /*worker_id=*/0);
  statistic.setId("benchmark_http_client.latency_2xx");

  statistic.recordValue(1500000);

  ASSERT_EQ(1, observed_.samples.size());
  EXPECT_EQ(1500, observed_.samples[0].value);
  EXPECT_EQ("cluster.0.benchmark_http_client.latency_2xx", observed_.samples[0].name);
  // Nighthawk's own view of the statistic is untouched by the adapter.
  EXPECT_EQ(1, statistic.count());
  EXPECT_EQ(Envoy::Stats::Histogram::Unit::Unspecified, statistic.unit());
}

TEST_F(EnvoyStatsSinkAdapterTest, CannotBeCreatedWithoutItsConfiguration) {
  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<NighthawkStatsSinkFactory>(
      "nighthawk.envoy_stats_sink_adapter");
  EXPECT_THROW_WITH_REGEX(factory.createStatsSink(store_.symbolTable()), Envoy::EnvoyException,
                          "must be created with its configuration");
}

TEST_F(EnvoyStatsSinkAdapterTest, RejectsAnEnvoySinkThatIsNotLinkedIn) {
  auto& factory = Envoy::Config::Utility::getAndCheckFactoryByName<NighthawkStatsSinkFactory>(
      "nighthawk.envoy_stats_sink_adapter");
  // Envoy resolves a factory by name and then by configuration type, so neither may match. No
  // Envoy stats sink is configured by the adapter's own configuration message.
  EXPECT_THROW_WITH_REGEX(
      factory.createStatsSink(adapterConfigFor("envoy.stat_sinks.no_such_sink",
                                               nighthawk::EnvoyStatsSinkAdapterConfig()),
                              context_),
      Envoy::EnvoyException, "envoy.stat_sinks.no_such_sink");
}

TEST_F(EnvoyStatsSinkAdapterTest, ReportsTheEnvoySinkFailingToBeCreated) {
  envoy_factory_.fail_creation_ = true;
  EXPECT_THROW_WITH_REGEX(createAdapter(), Envoy::EnvoyException,
                          "fake sink refused its configuration");
}

} // namespace
} // namespace Nighthawk
