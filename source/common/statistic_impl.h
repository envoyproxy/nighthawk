#pragma once

#include <memory>
#include <vector>

#include "nighthawk/common/statistic.h"

#include "hdr/hdr_histogram.h"
#include "source/common/common/logger.h"
#include "source/common/stats/histogram_impl.h"

#include "source/common/frequency.h"

namespace Nighthawk {

/**
 * Base class for all statistics implementations.
 */
class StatisticImpl : public Statistic, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  void addValue(uint64_t value) override;
  std::string toString() const override;
  nighthawk::client::Statistic toProto(SerializationDomain domain) const override;
  std::string id() const override;
  void setId(absl::string_view id) override;
  uint64_t count() const override;
  uint64_t max() const override;
  uint64_t min() const override;
  absl::StatusOr<std::unique_ptr<std::istream>> serializeNative() const override;
  absl::Status deserializeNative(std::istream&) override;

protected:
  std::string id_;
  uint64_t min_{UINT64_MAX};
  uint64_t max_{0};
  uint64_t count_{0};
};

/**
 * Dummy statistic for future use.
 * Intended be plugged into the system as a no-op in cases where statistic tracking
 * is not desired.
 */
class NullStatistic : public StatisticImpl {
public:
  void addValue(uint64_t) override {}
  double mean() const override { return 0.0; }
  double pvariance() const override { return 0.0; }
  double pstdev() const override { return 0.0; }
  StatisticPtr combine(const Statistic&) const override { return createNewInstanceOfSameType(); };
  uint64_t significantDigits() const override { return 0; }
  StatisticPtr createNewInstanceOfSameType() const override {
    return std::make_unique<NullStatistic>();
  };
};

/**
 * Simple statistic that keeps track of count/mean/pvariance/pstdev with low memory
 * requirements, but the potential for errors due to catastrophic cancellation.
 */
class SimpleStatistic : public StatisticImpl {
public:
  void addValue(uint64_t value) override;
  double mean() const override;
  double pvariance() const override;
  double pstdev() const override;
  StatisticPtr combine(const Statistic& statistic) const override;
  uint64_t significantDigits() const override { return 8; }
  StatisticPtr createNewInstanceOfSameType() const override {
    return std::make_unique<SimpleStatistic>();
  };
  absl::StatusOr<std::unique_ptr<std::istream>> serializeNative() const override;
  absl::Status deserializeNative(std::istream&) override;

private:
  double sum_x_{0};
  double sum_x2_{0};
};

/**
 * Statistic that keeps track of count/mean/pvariance/pstdev with low memory
 * requirements. Resistant to catastrophic cancellation and pretty accurate.
 * Based on Donald Knuth's online variance computation algorithm:
 * (Art of Computer Programming, Vol 2, page 232).
 * Knuth attributes this algorithm to B. P. Welford.
 * (Technometrics, Vol 4, No 3, Aug 1962 pp 419-420).
 */
class StreamingStatistic : public StatisticImpl {
public:
  void addValue(uint64_t value) override;
  double mean() const override;
  double pvariance() const override;
  double pstdev() const override;
  StatisticPtr combine(const Statistic& statistic) const override;
  bool resistsCatastrophicCancellation() const override { return true; }
  StatisticPtr createNewInstanceOfSameType() const override {
    return std::make_unique<StreamingStatistic>();
  };
  absl::StatusOr<std::unique_ptr<std::istream>> serializeNative() const override;
  absl::Status deserializeNative(std::istream&) override;

private:
  double mean_{0};
  double accumulated_variance_{0};
};

/**
 * InMemoryStatistic uses StreamingStatistic under the hood to compute statistics.
 * Stores the raw latencies in-memory, which may accumulate to a lot
 * of data(!). Not used right now, but useful for debugging purposes.
 */
class InMemoryStatistic : public StatisticImpl {
public:
  InMemoryStatistic();
  void addValue(uint64_t sample_value) override;
  double mean() const override;
  double pvariance() const override;
  double pstdev() const override;
  StatisticPtr combine(const Statistic& statistic) const override;
  bool resistsCatastrophicCancellation() const override {
    return streaming_stats_->resistsCatastrophicCancellation();
  }
  uint64_t significantDigits() const override { return streaming_stats_->significantDigits(); }
  StatisticPtr createNewInstanceOfSameType() const override {
    return std::make_unique<InMemoryStatistic>();
  };

private:
  std::vector<int64_t> samples_;
  StatisticPtr streaming_stats_;
};

/**
 * HdrStatistic uses HdrHistogram under the hood to compute statistics.
 */
class HdrStatistic : public StatisticImpl {
public:
  HdrStatistic();
  ~HdrStatistic() override;
  void addValue(uint64_t sample_value) override;
  uint64_t count() const override;
  double mean() const override;
  double pvariance() const override;
  double pstdev() const override;
  uint64_t max() const override;
  uint64_t min() const override;

  StatisticPtr combine(const Statistic& statistic) const override;
  nighthawk::client::Statistic toProto(SerializationDomain domain) const override;
  uint64_t significantDigits() const override { return SignificantDigits; }
  StatisticPtr createNewInstanceOfSameType() const override {
    return std::make_unique<HdrStatistic>();
  };

  absl::StatusOr<std::unique_ptr<std::istream>> serializeNative() const override;
  absl::Status deserializeNative(std::istream&) override;

private:
  static const int SignificantDigits;
  struct hdr_histogram* histogram_;
};

/**
 * CircllhistStatistic uses Circllhist under the hood to compute statistics.
 * Circllhist is used in the implementation of Envoy Histograms, compared to HdrHistogram it trades
 * precision for fast performance in merge and insertion. For more info, please see
 * https://github.com/circonus-labs/libcircllhist
 */
class CircllhistStatistic : public StatisticImpl {
public:
  CircllhistStatistic();
  ~CircllhistStatistic() override;

  void addValue(uint64_t value) override;
  double mean() const override;
  double pvariance() const override;
  double pstdev() const override;
  StatisticPtr combine(const Statistic& statistic) const override;
  // circllhist has low significant digit precision as a result of base 10
  // algorithm.
  uint64_t significantDigits() const override { return 1; }
  StatisticPtr createNewInstanceOfSameType() const override;
  nighthawk::client::Statistic toProto(SerializationDomain domain) const override;

private:
  histogram_t* histogram_;
};

/**
 * In order to be able to flush a histogram value to downstream Envoy stats Sinks, abstract class
 * SinkableStatistic takes the Scope reference in the constructor and wraps the
 * Envoy::Stats::HistogramHelper interface. Implementation of sinkable Nighthawk Statistic class
 * will inherit from this class.
 */
class SinkableStatistic : public Envoy::Stats::HistogramImplHelper {
public:
  // Calling HistogramImplHelper(SymbolTable& symbol_table) constructor to construct an empty
  // MetricImpl. This is to bypass the complicated logic of setting up SymbolTable/StatName in
  // Envoy.
  SinkableStatistic(Envoy::Stats::Scope& scope, std::optional<int> worker_id);
  ~SinkableStatistic() override;

  // Envoy::Stats::Histogram::Unit is {Unspecified, Bytes, Microseconds, Milliseconds, Percent};
  // there is no nanosecond unit, which is what Nighthawk::Statistic records. Microseconds is
  // returned, and samples are divided on the way into the store histogram, so the mirror is
  // self-describing: a sink can scale it by unit instead of knowing Nighthawk's convention.
  // Unspecified would be honest about the resolution and useless to every sink -- Envoy's statsd
  // sinks label histograms "|ms" and scale only by unit, so an unspecified nanosecond histogram is
  // exported as milliseconds a factor of 10^6 out.
  //
  // What the conversion costs depends on which kind of sink is reading, and the two differ:
  //
  //   - Sinks that read the merged histogram statistics -- Prometheus, the admin endpoint,
  //     OpenTelemetry, the gRPC metrics service -- see values already binned by libcircllhist,
  //     whose buckets are val x 10^exp with val in [10, 99]: two significant decimal digits, 90
  //     bins per decade. Above about 10 us those bins are coarser than a microsecond, so the
  //     conversion removes nothing that would have survived anyway.
  //   - Sinks implementing onHistogramComplete(), which is how the statsd sinks consume
  //     histograms, see every sample exactly as recorded. ParentHistogramImpl::recordValue()
  //     records into the thread local circllhist and then calls deliverHistogramToSinks() with
  //     the raw value, and UdpStatsdSink::flush() never reads snapshot.histograms(). Nothing
  //     bins the value between here and the wire, so on that path microseconds is the real
  //     precision floor: a 40.7 us sample leaves as 41 us and the remainder is gone.
  //
  // That is the path this change exists to serve, so the loss is not hypothetical: roughly 1
  // percent at 40 us and proportionally worse below, against about 0.02 percent for the
  // millisecond scale request latencies Nighthawk actually measures. Rounding rather than
  // truncating halves it. A sample under half a microsecond reaches the mirror as zero.
  //
  // A nanosecond unit upstream would remove the loss on that path; there is no such unit today.
  // This is the floor of what Envoy can currently express, not a considered limit, and should
  // not be written down as one.
  //
  // None of this affects what Nighthawk reports: its output is rendered from the HdrHistogram or
  // Circllhist data, which keeps nanoseconds.
  //
  // This is a property of the class, and it is only correct because every SinkableStatistic is a
  // latency. The response size statistics are byte counts, not durations, and are deliberately
  // not sinkable -- BenchmarkClientStatistic is handed StreamingStatistic for those two. If they
  // or anything else non-temporal ever become sinkable, this has to become per statistic:
  // declaring a byte count as Microseconds and dividing it by a thousand turns a 10 byte body
  // into 0 and reports it as a duration, and nothing here would fail to make that visible.
  Envoy::Stats::Histogram::Unit unit() const override;
  Envoy::Stats::SymbolTable& symbolTable() override;
  // Return the id of the worker where this statistic is defined. Per worker
  // statistic should always set worker_id. Return std::nullopt when the
  // statistic is not defined per worker.
  const std::optional<int> worker_id() const { return worker_id_; }

protected:
  /**
   * The Envoy store histogram that mirrors this statistic, created on first use. Recording a
   * sample into it both feeds the snapshot that flush based stats sinks read and delivers the
   * sample to sinks implementing onHistogramComplete(), because
   * Envoy::Stats::ParentHistogramImpl::recordValue() calls deliverHistogramToSinks() itself.
   *
   * It is created lazily rather than in the constructor because a statistic's id, which names the
   * histogram, is assigned after construction. The scope is the worker's ("cluster.<n>."), so the
   * histogram is named "cluster.<n>.<id>" and stats sinks can recover the worker from the name.
   *
   * Nighthawk's own output is unaffected: it is rendered from this statistic's HdrHistogram or
   * Circllhist data, not from the mirror, which is why the mirror can carry the coarser
   * representation that unit() describes rather than the nanoseconds recorded here.
   */
  Envoy::Stats::Histogram& storeHistogram();

  /**
   * Points storeHistogram() at the store histogram named by the statistic's current id. Called
   * when the id is assigned, so that the mirror is named correctly regardless of whether any
   * sample was recorded before that.
   */
  void bindStoreHistogram();

  /**
   * Converts a sample from the nanoseconds Nighthawk records to the microseconds the store
   * histogram declares. See unit() for why the mirror is not recorded in nanoseconds.
   *
   * @param nanoseconds the sample as recorded by Nighthawk.
   * @return uint64_t the same sample in whole microseconds.
   */
  static uint64_t toStoreHistogramUnit(uint64_t nanoseconds);

  // This is used in child class for delivering the histogram data to sinks.
  Envoy::Stats::Scope& scope_;

private:
  // worker_id can be used in downstream stats Sinks as the stats tag.
  std::optional<int> worker_id_;
  // Owned by the store; see storeHistogram().
  Envoy::Stats::Histogram* store_histogram_{nullptr};
};

// Implementation of sinkable Nighthawk Statistic with HdrHistogram.
class SinkableHdrStatistic : public SinkableStatistic, public HdrStatistic {
public:
  // The constructor takes the Scope reference which is used to flush a histogram value to
  // downstream stats Sinks through deliverHistogramToSinks().
  SinkableHdrStatistic(Envoy::Stats::Scope& scope, std::optional<int> worker_id = std::nullopt);

  // Envoy::Stats::Histogram
  void recordValue(uint64_t value) override;
  void markUnused() override {}
  bool used() const override { return count() > 0; }
  bool hidden() const override { return false; }
  // Overriding name() to return Nighthawk::Statistic::id().
  std::string name() const override { return id(); }
  // Overriding tagExtractedName() to return string(worker_id) + "." + Nighthawk::Statistic::id()
  // when worker_id is set. The worker_id prefix can be used in customized stats sinks.
  std::string tagExtractedName() const override;

  // Nighthawk::Statistic
  void addValue(uint64_t value) override { recordValue(value); }
  // Rebinds the store histogram, which is named after the id.
  void setId(absl::string_view id) override;
};

// Implementation of sinkable Nighthawk Statistic with Circllhist Histogram.
class SinkableCircllhistStatistic : public SinkableStatistic, public CircllhistStatistic {
public:
  // The constructor takes the Scope reference which is used to flush a histogram value to
  // downstream stats Sinks through deliverHistogramToSinks().
  SinkableCircllhistStatistic(Envoy::Stats::Scope& scope,
                              std::optional<int> worker_id = std::nullopt);

  // Envoy::Stats::Histogram
  void recordValue(uint64_t value) override;
  void markUnused() override {}
  bool used() const override { return count() > 0; }
  bool hidden() const override { return false; }
  // Overriding name() to return Nighthawk::Statistic::id().
  std::string name() const override { return id(); }
  // Overriding tagExtractedName() to return string(worker_id) + "." + Nighthawk::Statistic::id()
  // when worker_id is set. The worker_id prefix can be used in customized stats sinks.
  std::string tagExtractedName() const override;

  // Nighthawk::Statistic
  void addValue(uint64_t value) override { recordValue(value); }
  // Rebinds the store histogram, which is named after the id.
  void setId(absl::string_view id) override;
};

} // namespace Nighthawk
