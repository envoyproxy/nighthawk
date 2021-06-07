#pragma once

#include <memory>
#include <vector>

#include "nighthawk/common/statistic.h"

#include "external/dep_hdrhistogram_c/src/hdr_histogram.h"
#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/stats/histogram_impl.h"

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
  SinkableStatistic(Envoy::Stats::Scope& scope, absl::optional<int> worker_id);
  ~SinkableStatistic() override;

  // Currently Envoy Histogram Unit supports {Unspecified, Bytes, Microseconds, Milliseconds}. By
  // default, Nighthawk::Statistic uses nanosecond as the unit of latency histograms, so Unspecified
  // is returned here to isolate Nighthawk Statistic from Envoy Histogram Unit.
  Envoy::Stats::Histogram::Unit unit() const override;
  Envoy::Stats::SymbolTable& symbolTable() override;
  // Return the id of the worker where this statistic is defined. Per worker
  // statistic should always set worker_id. Return absl::nullopt when the
  // statistic is not defined per worker.
  const absl::optional<int> worker_id() const { return worker_id_; }

protected:
  // This is used in child class for delivering the histogram data to sinks.
  Envoy::Stats::Scope& scope_;

private:
  // worker_id can be used in downstream stats Sinks as the stats tag.
  absl::optional<int> worker_id_;
};

// Implementation of sinkable Nighthawk Statistic with HdrHistogram.
class SinkableHdrStatistic : public SinkableStatistic, public HdrStatistic {
public:
  // The constructor takes the Scope reference which is used to flush a histogram value to
  // downstream stats Sinks through deliverHistogramToSinks().
  SinkableHdrStatistic(Envoy::Stats::Scope& scope, absl::optional<int> worker_id = absl::nullopt);

  // Envoy::Stats::Histogram
  void recordValue(uint64_t value) override;
  bool used() const override { return count() > 0; }
  // Overriding name() to return Nighthawk::Statistic::id().
  std::string name() const override { return id(); }
  // Overriding tagExtractedName() to return string(worker_id) + "." + Nighthawk::Statistic::id()
  // when worker_id is set. The worker_id prefix can be used in customized stats sinks.
  std::string tagExtractedName() const override;

  // Nighthawk::Statistic
  void addValue(uint64_t value) override { recordValue(value); }
};

// Implementation of sinkable Nighthawk Statistic with Circllhist Histogram.
class SinkableCircllhistStatistic : public SinkableStatistic, public CircllhistStatistic {
public:
  // The constructor takes the Scope reference which is used to flush a histogram value to
  // downstream stats Sinks through deliverHistogramToSinks().
  SinkableCircllhistStatistic(Envoy::Stats::Scope& scope,
                              absl::optional<int> worker_id = absl::nullopt);

  // Envoy::Stats::Histogram
  void recordValue(uint64_t value) override;
  bool used() const override { return count() > 0; }
  // Overriding name() to return Nighthawk::Statistic::id().
  std::string name() const override { return id(); }
  // Overriding tagExtractedName() to return string(worker_id) + "." + Nighthawk::Statistic::id()
  // when worker_id is set. The worker_id prefix can be used in customized stats sinks.
  std::string tagExtractedName() const override;

  // Nighthawk::Statistic
  void addValue(uint64_t value) override { recordValue(value); }
};

} // namespace Nighthawk
