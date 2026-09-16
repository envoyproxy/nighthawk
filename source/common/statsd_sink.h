#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "envoy/config/metrics/v3/stats.pb.h"
#include "envoy/network/address.h"
#include "envoy/network/io_handle.h"
#include "envoy/stats/sink.h"
#include "envoy/thread_local/thread_local.h"

#include "nighthawk/common/factories.h"

#include "external/envoy/source/common/common/logger.h"

#include "absl/synchronization/mutex.h"

namespace Nighthawk {

/**
 * Stats sink that pushes Nighthawk's counters, gauges and latency samples to a statsd (or
 * DogStatsD) receiver over UDP. Metric names are prefixed (default "nighthawk"). Counters are
 * sent as deltas ("|c") on every flush, gauges as values ("|g"). Each latency sample recorded in
 * a sinkable Nighthawk statistic (e.g. benchmark_http_client.latency_2xx,
 * benchmark_stream.message_latency) is sent as a timing in milliseconds with sub-millisecond
 * precision ("|ms"), Envoy's own byte histograms as "|h".
 *
 * Nighthawk keeps per-worker stats under "cluster.<n>." / "worker.<n>." prefixes. With tags
 * enabled (DogStatsD) that prefix is stripped and emitted as a "worker:<n>" tag; without tags all
 * per-worker metrics, latency samples included, are named "worker.<n>.<rest>".
 */
class StatsdSink : public Envoy::Stats::Sink,
                   public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  /**
   * @param address the UDP address of the statsd receiver. One UDP socket is shared by all
   * threads (the receiver keys aggregation by client address, so one client per process).
   * @param prefix metric name prefix; "nighthawk" when empty.
   * @param use_tags emit DogStatsD tags ("|#worker:0") instead of name prefixes.
   * @param max_bytes_per_datagram batch messages into newline-separated datagrams of up to this
   * size; one message per datagram when unset. Counters and gauges are batched per flush;
   * latency samples are batched per recording thread and sent whenever the batch is full, on
   * the next stats flush, and when the sink is destroyed.
   * @param extra_tags "key:value" tags appended to every message when use_tags is set.
   */
  StatsdSink(Envoy::Network::Address::InstanceConstSharedPtr address, absl::string_view prefix,
             bool use_tags, std::optional<uint64_t> max_bytes_per_datagram,
             const std::vector<std::string>& extra_tags = {});
  ~StatsdSink() override;

  // Envoy::Stats::Sink
  void flush(Envoy::Stats::MetricSnapshot& snapshot) override;
  void onHistogramComplete(const Envoy::Stats::Histogram& histogram, uint64_t value) override;

  /**
   * Builds the statsd message for a metric, exposed for tests.
   * @param store_name the metric name as known to the stats store.
   * @param value formatted value, e.g. "12" or "1.234".
   * @param type statsd type suffix, e.g. "c", "g", "ms".
   * @param worker_id worker the metric belongs to, if it is a per-worker metric.
   */
  std::string buildMessage(absl::string_view store_name, absl::string_view value,
                           absl::string_view type, std::optional<int> worker_id) const;

  /**
   * Formats a Nighthawk latency (nanoseconds) as milliseconds with microsecond precision.
   */
  static std::string formatNanosAsMillis(uint64_t nanos);

  const std::string& prefix() const { return prefix_; }
  bool useTags() const { return use_tags_; }

private:
  // A per-thread accumulator of latency-sample messages, flushed as one datagram.
  struct SampleBatch {
    std::string data;
  };

  // Splits "cluster.<n>.rest" / "worker.<n>.rest" into (rest, n).
  static std::pair<std::string, std::optional<int>> splitWorkerPrefix(absl::string_view name);
  void send(absl::string_view message);
  void sendBatch(const std::vector<std::string>& messages);
  // Appends a sample message to the calling thread's batch, sending when it is full.
  void enqueueSample(std::string message);
  // Sends the calling thread's pending batch, if any.
  void flushSamples();
  // Sends every thread's pending batch; called on destruction, after the worker threads stopped.
  void flushAllSamples();

  const Envoy::Network::Address::InstanceConstSharedPtr address_;
  Envoy::Network::IoHandlePtr io_handle_;
  absl::Mutex socket_mutex_;
  // Batches keyed by the recording thread, guarded by batches_mutex_. Each thread only ever
  // touches its own entry while running; flushAllSamples() walks all of them at shutdown.
  absl::Mutex batches_mutex_;
  std::map<std::thread::id, std::shared_ptr<SampleBatch>> batches_ ABSL_GUARDED_BY(batches_mutex_);
  const std::string prefix_;
  const bool use_tags_;
  const std::optional<uint64_t> max_bytes_per_datagram_;
  // Pre-rendered ",key:value,..." suffix appended after the worker tag (empty without tags).
  const std::string extra_tags_;
};

/**
 * Factory for "envoy.stat_sinks.statsd" (envoy.config.metrics.v3.StatsdSink). Only the UDP
 * `address` form is supported; `tcp_cluster_name` is rejected.
 */
class StatsdSinkFactory : public NighthawkStatsSinkFactory {
public:
  std::unique_ptr<Envoy::Stats::Sink>
  createStatsSink(const Envoy::Protobuf::Message& config, Envoy::Stats::SymbolTable& symbol_table,
                  Envoy::ThreadLocal::SlotAllocator& tls,
                  const std::vector<std::string>& tags) override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  std::string name() const override { return "envoy.stat_sinks.statsd"; }
};

/**
 * Factory for "envoy.stat_sinks.dog_statsd" (envoy.config.metrics.v3.DogStatsdSink): the same
 * sink with DogStatsD tags enabled and optional datagram batching.
 */
class DogStatsdSinkFactory : public NighthawkStatsSinkFactory {
public:
  std::unique_ptr<Envoy::Stats::Sink>
  createStatsSink(const Envoy::Protobuf::Message& config, Envoy::Stats::SymbolTable& symbol_table,
                  Envoy::ThreadLocal::SlotAllocator& tls,
                  const std::vector<std::string>& tags) override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override;
  std::string name() const override { return "envoy.stat_sinks.dog_statsd"; }
};

} // namespace Nighthawk
