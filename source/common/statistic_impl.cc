#include "common/statistic_impl.h"

#include <cmath>
#include <cstdio>
#include <sstream>

#include "external/envoy/source/common/common/assert.h"

namespace Nighthawk {

std::string StatisticImpl::toString() const {
  return toProto(SerializationDomain::RAW).DebugString();
}

nighthawk::client::Statistic StatisticImpl::toProto(SerializationDomain domain) const {
  nighthawk::client::Statistic statistic;

  statistic.set_id(id());
  statistic.set_count(count());
  if (domain == Statistic::SerializationDomain::DURATION) {
    int64_t nanos;
    nanos = count() == 0 ? 0 : static_cast<int64_t>(std::round(mean()));
    statistic.mutable_mean()->set_seconds(nanos / 1000000000);
    statistic.mutable_mean()->set_nanos(nanos % 1000000000);
    nanos =
        count() == 0 ? 0 : static_cast<int64_t>(std::round(std::isnan(pstdev()) ? 0 : pstdev()));
    statistic.mutable_pstdev()->set_seconds(nanos / 1000000000);
    statistic.mutable_pstdev()->set_nanos(nanos % 1000000000);
    nanos = min();
    statistic.mutable_min()->set_seconds(nanos / 1000000000);
    statistic.mutable_min()->set_nanos(nanos % 1000000000);
    nanos = max();
    statistic.mutable_max()->set_seconds(nanos / 1000000000);
    statistic.mutable_max()->set_nanos(nanos % 1000000000);
  } else {
    statistic.set_raw_mean(mean());
    statistic.set_raw_pstdev(pstdev());
    statistic.set_raw_min(min());
    statistic.set_raw_max(max());
  }

  return statistic;
}

std::string StatisticImpl::id() const { return id_; };

void StatisticImpl::setId(absl::string_view id) { id_ = std::string(id); };

void StatisticImpl::addValue(uint64_t value) {
  min_ = value < min_ ? value : min_;
  max_ = value > max_ ? value : max_;
  count_++;
};

uint64_t StatisticImpl::count() const { return count_; }

uint64_t StatisticImpl::min() const { return min_; };

uint64_t StatisticImpl::max() const { return max_; };

void SimpleStatistic::addValue(uint64_t value) {
  StatisticImpl::addValue(value);
  sum_x_ += value;
  sum_x2_ += 1.0 * value * value;
}

double SimpleStatistic::mean() const { return count() == 0 ? std::nan("") : sum_x_ / count_; }

double SimpleStatistic::pvariance() const {
  return count() == 0 ? std::nan("") : (sum_x2_ / count_) - (mean() * mean());
}

double SimpleStatistic::pstdev() const { return count() == 0 ? std::nan("") : sqrt(pvariance()); }

StatisticPtr SimpleStatistic::combine(const Statistic& statistic) const {
  const SimpleStatistic& a = *this;
  const auto& b = dynamic_cast<const SimpleStatistic&>(statistic);
  auto combined = std::make_unique<SimpleStatistic>();
  combined->min_ = a.min() > b.min() ? b.min() : a.min();
  combined->max_ = a.max() > b.max() ? a.max() : b.max();
  combined->count_ = a.count() + b.count();
  combined->sum_x_ = a.sum_x_ + b.sum_x_;
  combined->sum_x2_ = a.sum_x2_ + b.sum_x2_;
  return combined;
}

void StreamingStatistic::addValue(uint64_t value) {
  double delta, delta_n;
  StatisticImpl::addValue(value);
  delta = value - mean_;
  delta_n = delta / count_;
  mean_ += delta_n;
  accumulated_variance_ += delta * delta_n * (count_ - 1.0);
}

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

  combined->min_ = a.min() > b.min() ? b.min() : a.min();
  combined->max_ = a.max() > b.max() ? a.max() : b.max();
  combined->count_ = a.count() + b.count();
  // A statistic instance with zero samples will return std::isnan() as its mean.
  // For the the merge we are doing here we need to treat that as 0.
  auto a_mean = std::isnan(a.mean()) ? 0 : a.mean();
  auto b_mean = std::isnan(b.mean()) ? 0 : b.mean();
  combined->mean_ = ((a.count() * a_mean) + (b.count() * b_mean)) / combined->count_;
  combined->accumulated_variance_ =
      a.accumulated_variance_ + b.accumulated_variance_ +
      pow(a_mean - b_mean, 2) * a.count() * b.count() / combined->count();
  return combined;
}

InMemoryStatistic::InMemoryStatistic() : streaming_stats_(std::make_unique<StreamingStatistic>()) {}

void InMemoryStatistic::addValue(uint64_t sample_value) {
  StatisticImpl::addValue(sample_value);
  samples_.push_back(sample_value);
  streaming_stats_->addValue(sample_value);
}

double InMemoryStatistic::mean() const { return streaming_stats_->mean(); }
double InMemoryStatistic::pvariance() const { return streaming_stats_->pvariance(); }
double InMemoryStatistic::pstdev() const { return streaming_stats_->pstdev(); }

StatisticPtr InMemoryStatistic::combine(const Statistic& statistic) const {
  auto combined = std::make_unique<InMemoryStatistic>();
  const auto& b = dynamic_cast<const InMemoryStatistic&>(statistic);

  combined->min_ = this->min() > b.min() ? b.min() : this->min();
  combined->max_ = this->max() > b.max() ? this->max() : b.max();
  combined->samples_.insert(combined->samples_.end(), this->samples_.begin(), this->samples_.end());
  combined->samples_.insert(combined->samples_.end(), b.samples_.begin(), b.samples_.end());
  combined->streaming_stats_ = this->streaming_stats_->combine(*b.streaming_stats_);
  combined->count_ = combined->samples_.size();
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

void HdrStatistic::addValue(uint64_t value) {
  // Failure to record a value can happen when it exceeds the configured minimum
  // or maximum value we passed when initializing histogram_.
  if (!hdr_record_value(histogram_, value)) {
    ENVOY_LOG(warn, "Failed to record value into HdrHistogram.");
  } else {
    StatisticImpl::addValue(value);
  }
}

// We override count for the Hdr statistics, because it may have dropped
// out of range values. hence our own tracking may be inaccurate.
uint64_t HdrStatistic::count() const { return histogram_->total_count; }
double HdrStatistic::mean() const { return count() == 0 ? std::nan("") : hdr_mean(histogram_); }
double HdrStatistic::pvariance() const { return pstdev() * pstdev(); }
double HdrStatistic::pstdev() const { return count() == 0 ? std::nan("") : hdr_stddev(histogram_); }
uint64_t HdrStatistic::min() const {
  return count() == 0 ? UINT64_MAX : hdr_value_at_percentile(histogram_, 0);
}
uint64_t HdrStatistic::max() const { return hdr_value_at_percentile(histogram_, 100); }

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

nighthawk::client::Statistic HdrStatistic::toProto(SerializationDomain domain) const {
  nighthawk::client::Statistic proto = StatisticImpl::toProto(domain);

  struct hdr_iter iter;
  struct hdr_iter_percentiles* percentiles;
  hdr_iter_percentile_init(&iter, histogram_, 5 /*ticks_per_half_distance*/);

  percentiles = &iter.specifics.percentiles;
  while (hdr_iter_next(&iter)) {
    nighthawk::client::Percentile* percentile;

    percentile = proto.add_percentiles();
    if (domain == Statistic::SerializationDomain::DURATION) {
      percentile->mutable_duration()->set_seconds(iter.highest_equivalent_value / 1000000000);
      percentile->mutable_duration()->set_nanos(iter.highest_equivalent_value % 1000000000);
    } else {
      percentile->set_raw_value(iter.highest_equivalent_value);
    }
    percentile->set_percentile(percentiles->percentile / 100.0);
    percentile->set_count(iter.cumulative_count);
  }

  return proto;
}

} // namespace Nighthawk