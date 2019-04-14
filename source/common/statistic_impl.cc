
#include "common/statistic_impl.h"

#include <cmath>
#include <cstdio>
#include <sstream>

#include "common/common/assert.h"

namespace Nighthawk {

std::string StatisticImpl::toString() const {
  return fmt::format("Count: {}. Mean: {:.{}f} μs. pstdev: {:.{}f} μs.\n", count(), mean() / 1000,
                     2, pstdev() / 1000, 2);
}

nighthawk::client::Statistic StatisticImpl::toProto() {
  nighthawk::client::Statistic statistic;

  statistic.set_id(id());
  statistic.set_count(count());

  int64_t nanos = count() == 0 ? 0 : std::round(mean());
  statistic.mutable_mean()->set_seconds(nanos / 1000000000);
  statistic.mutable_mean()->set_nanos(nanos % 1000000000);

  nanos = count() == 0 ? 0 : std::round(pstdev());
  statistic.mutable_pstdev()->set_seconds(nanos / 1000000000);
  statistic.mutable_pstdev()->set_nanos(nanos % 1000000000);

  return statistic;
}

std::string StatisticImpl::id() const { return id_; };

void StatisticImpl::setId(absl::string_view id) { id_ = std::string(id); };

SimpleStatistic::SimpleStatistic() : count_(0), sum_x_(0), sum_x2_(0) {}

void SimpleStatistic::addValue(int64_t value) {
  count_++;
  sum_x_ += value;
  sum_x2_ += value * value;
}

uint64_t SimpleStatistic::count() const { return count_; }

double SimpleStatistic::mean() const { return count() == 0 ? std::nan("") : sum_x_ / count_; }

double SimpleStatistic::pvariance() const {
  return count() == 0 ? std::nan("") : (sum_x2_ / count_) - (mean() * mean());
}

double SimpleStatistic::pstdev() const { return count() == 0 ? std::nan("") : sqrt(pvariance()); }

StatisticPtr SimpleStatistic::combine(const Statistic& statistic) const {
  const SimpleStatistic& a = *this;
  const auto& b = dynamic_cast<const SimpleStatistic&>(statistic);
  auto combined = std::make_unique<SimpleStatistic>();

  combined->count_ = a.count() + b.count();
  combined->sum_x_ = a.sum_x_ + b.sum_x_;
  combined->sum_x2_ = a.sum_x2_ + b.sum_x2_;
  return combined;
}

StreamingStatistic::StreamingStatistic() : count_(0), mean_(0), accumulated_variance_(0) {}

void StreamingStatistic::addValue(int64_t value) {
  double delta, delta_n;
  count_++;
  delta = value - mean_;
  delta_n = delta / count_;
  mean_ += delta_n;
  accumulated_variance_ += delta * delta_n * (count_ - 1);
}

uint64_t StreamingStatistic::count() const { return count_; }

double StreamingStatistic::mean() const { return count_ == 0 ? std::nan("") : mean_; }

double StreamingStatistic::pvariance() const {
  return count() == 0 ? std::nan("") : accumulated_variance_ / count_;
}

double StreamingStatistic::pstdev() const {
  return count() == 0 ? std::nan("") : sqrt(pvariance());
}

StatisticPtr StreamingStatistic::combine(const Statistic& statistic) const {
  const StreamingStatistic& a = *this;
  const auto& b = dynamic_cast<const StreamingStatistic&>(statistic);
  auto combined = std::make_unique<StreamingStatistic>();

  combined->count_ = a.count() + b.count();
  combined->mean_ = ((a.count() * a.mean()) + (b.count() * b.mean())) / combined->count_;
  combined->accumulated_variance_ =
      a.accumulated_variance_ + b.accumulated_variance_ +
      pow(a.mean() - b.mean(), 2) * a.count() * b.count() / combined->count();
  return combined;
}

InMemoryStatistic::InMemoryStatistic() : streaming_stats_(std::make_unique<StreamingStatistic>()) {}

void InMemoryStatistic::addValue(int64_t sample_value) {
  samples_.push_back(sample_value);
  streaming_stats_->addValue(sample_value);
}

uint64_t InMemoryStatistic::count() const {
  ASSERT(streaming_stats_->count() == samples_.size());
  return streaming_stats_->count();
}
double InMemoryStatistic::mean() const { return streaming_stats_->mean(); }
double InMemoryStatistic::pvariance() const { return streaming_stats_->pvariance(); }
double InMemoryStatistic::pstdev() const { return streaming_stats_->pstdev(); }

StatisticPtr InMemoryStatistic::combine(const Statistic& statistic) const {
  auto combined = std::make_unique<InMemoryStatistic>();
  const auto& b = dynamic_cast<const InMemoryStatistic&>(statistic);

  combined->samples_.insert(combined->samples_.end(), this->samples_.begin(), this->samples_.end());
  combined->samples_.insert(combined->samples_.end(), b.samples_.begin(), b.samples_.end());
  combined->streaming_stats_ = this->streaming_stats_->combine(*b.streaming_stats_);
  return combined;
}

const int HdrStatistic::SignificantDigits = 4;

HdrStatistic::HdrStatistic() : histogram_(nullptr) {
  // Upper bound of 60 seconds (tracking in nanoseconds).
  const uint64_t max_latency = 1000L * 1000 * 1000 * 60;

  int status = hdr_init(1 /* min trackable value */, max_latency, HdrStatistic::SignificantDigits,
                        &histogram_);
  ASSERT(status == 0);
  ASSERT(histogram_ != nullptr);
}

// TODO(oschaaf): valgrind complains when a Histogram is created but never used.
HdrStatistic::~HdrStatistic() {
  ASSERT(histogram_ != nullptr);
  hdr_close(histogram_);
  histogram_ = nullptr;
}

void HdrStatistic::addValue(int64_t value) {
  // Failure to record a value can happen when it exceeds the configured minimum
  // or maximum value we passed when initializing histogram_.
  if (!hdr_record_value(histogram_, value)) {
    ENVOY_LOG(warn, "Failed to record value into HdrHistogram.");
  }
}

uint64_t HdrStatistic::count() const { return histogram_->total_count; }
double HdrStatistic::mean() const { return count() == 0 ? std::nan("") : hdr_mean(histogram_); }
double HdrStatistic::pvariance() const { return pstdev() * pstdev(); }
double HdrStatistic::pstdev() const { return count() == 0 ? std::nan("") : hdr_stddev(histogram_); }

StatisticPtr HdrStatistic::combine(const Statistic& statistic) const {
  auto combined = std::make_unique<HdrStatistic>();
  const auto& b = dynamic_cast<const HdrStatistic&>(statistic);

  // Dropping a value can happen when it exceeds the configured minimum
  // or maximum value we passed when initializing histogram_.
  int dropped;
  dropped = hdr_add(combined->histogram_, this->histogram_);
  dropped += hdr_add(combined->histogram_, b.histogram_);
  if (dropped > 0) {
    ENVOY_LOG(warn, "Combining HdrHistograms dropped values.");
  }
  return combined;
}

std::string HdrStatistic::toString() const {
  std::stringstream stream;

  stream << StatisticImpl::toString();
  stream << fmt::format("{:>12} {:>14} (usec)", "Percentile", "Value") << std::endl;

  std::vector<double> percentiles{50.0, 75.0, 90.0, 99.0, 99.9, 99.99, 99.999, 100.0};
  for (double p : percentiles) {
    const int64_t n = hdr_value_at_percentile(histogram_, p);

    // We scale from nanoseconds to microseconds in the output.
    stream << fmt::format("{:>12}% {:>14}", p, n / 1000.0) << std::endl;
  }
  return stream.str();
}

nighthawk::client::Statistic HdrStatistic::toProto() {
  nighthawk::client::Statistic proto = StatisticImpl::toProto();

  struct hdr_iter iter;
  struct hdr_iter_percentiles* percentiles;
  hdr_iter_percentile_init(&iter, histogram_, 5 /*ticks_per_half_distance*/);

  percentiles = &iter.specifics.percentiles;
  while (hdr_iter_next(&iter)) {
    nighthawk::client::Percentile* percentile;

    percentile = proto.add_percentiles();

    percentile->mutable_duration()->set_seconds(iter.highest_equivalent_value / 1000000000);
    percentile->mutable_duration()->set_nanos(iter.highest_equivalent_value % 1000000000);

    percentile->set_percentile(percentiles->percentile / 100.0);
    percentile->set_count(iter.cumulative_count);
  }

  return proto;
}

} // namespace Nighthawk