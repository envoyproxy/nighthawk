#pragma once

#include <memory>
#include <vector>

#include "nighthawk/common/statistic.h"

#include "common/common/logger.h"
#include "common/frequency.h"

#include "external/dep_hdrhistogram_c/src/hdr_histogram.h"

namespace Nighthawk {

class StatisticImpl : public Statistic, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  std::string toString() const override;
  nighthawk::client::Statistic toProto() override;
  std::string id() const override;
  void setId(absl::string_view id) override;
  std::string id_;
};

/**
 * Simple statistic that keeps track of count/mean/pvariance/pstdev with low memory
 * requirements, but the potential for errors due to catastrophic cancellation.
 */
class SimpleStatistic : public StatisticImpl {
public:
  SimpleStatistic();
  void addValue(uint64_t value) override;
  uint64_t count() const override;
  double mean() const override;
  double pvariance() const override;
  double pstdev() const override;
  StatisticPtr combine(const Statistic& statistic) const override;
  uint64_t significantDigits() const override { return 8; }

private:
  uint64_t count_;
  double sum_x_;
  double sum_x2_;
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
  StreamingStatistic();
  void addValue(uint64_t value) override;
  uint64_t count() const override;
  double mean() const override;
  double pvariance() const override;
  double pstdev() const override;
  StatisticPtr combine(const Statistic& statistic) const override;
  bool resistsCatastrophicCancellation() const override { return true; }

private:
  uint64_t count_;
  double mean_;
  double accumulated_variance_;
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
  uint64_t count() const override;
  double mean() const override;
  double pvariance() const override;
  double pstdev() const override;
  StatisticPtr combine(const Statistic& statistic) const override;
  bool resistsCatastrophicCancellation() const override {
    return streaming_stats_->resistsCatastrophicCancellation();
  }
  uint64_t significantDigits() const override { return streaming_stats_->significantDigits(); }

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

  StatisticPtr combine(const Statistic& statistic) const override;
  std::string toString() const override;
  nighthawk::client::Statistic toProto() override;
  uint64_t significantDigits() const override { return SignificantDigits; }

private:
  static const int SignificantDigits;
  struct hdr_histogram* histogram_;
};

} // namespace Nighthawk