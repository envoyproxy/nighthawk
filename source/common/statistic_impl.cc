#include "common/statistic_impl.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "external/dep_hdrhistogram_c/src/hdr_histogram_log.h"
#include "external/envoy/source/common/common/assert.h"
#include "external/envoy/source/common/protobuf/utility.h"

#include "internal_proto/statistic/statistic.pb.h"

namespace Nighthawk {

namespace {

/**
 * @param mutable_duration The proto duration that will be updated to reflect the passed in nanos.
 * @param nanos The number of nanoseconds.
 */
static void setDurationFromNanos(Envoy::ProtobufWkt::Duration& mutable_duration,
                                 const uint64_t nanos) {
  constexpr uint64_t one_billion = 1e9;
  mutable_duration.set_seconds(nanos / one_billion);
  mutable_duration.set_nanos(nanos % one_billion);
}

} // namespace

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
    setDurationFromNanos(*statistic.mutable_mean(), nanos);
    nanos =
        count() == 0 ? 0 : static_cast<int64_t>(std::round(std::isnan(pstdev()) ? 0 : pstdev()));
    setDurationFromNanos(*statistic.mutable_pstdev(), nanos);
    setDurationFromNanos(*statistic.mutable_min(), min() == UINT64_MAX ? 0 : min());
    setDurationFromNanos(*statistic.mutable_max(), max());
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
  min_ = std::min(min_, value);
  max_ = std::max(max_, value);
  count_++;
};

uint64_t StatisticImpl::count() const { return count_; }

uint64_t StatisticImpl::min() const { return min_; };

uint64_t StatisticImpl::max() const { return max_; };

absl::StatusOr<std::unique_ptr<std::istream>> StatisticImpl::serializeNative() const {
  return absl::Status(absl::StatusCode::kUnimplemented, "serializeNative not implemented.");
}

absl::Status StatisticImpl::deserializeNative(std::istream&) {
  return absl::Status(absl::StatusCode::kUnimplemented, "deserializeNative not implemented.");
}

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
  combined->min_ = std::min(a.min(), b.min());
  combined->max_ = std::max(a.max(), b.max());
  combined->count_ = a.count() + b.count();
  combined->sum_x_ = a.sum_x_ + b.sum_x_;
  combined->sum_x2_ = a.sum_x2_ + b.sum_x2_;
  return combined;
}

absl::StatusOr<std::unique_ptr<std::istream>> SimpleStatistic::serializeNative() const {
  nighthawk::internal::SimpleStatistic proto;
  proto.set_id(id());
  proto.set_count(count());
  proto.set_min(min());
  proto.set_max(max());
  proto.set_sum_x(sum_x_);
  proto.set_sum_x_2(sum_x2_);

  std::string tmp;
  proto.SerializeToString(&tmp);
  auto write_stream = std::make_unique<std::stringstream>();
  *write_stream << tmp;
  return write_stream;
}

absl::Status SimpleStatistic::deserializeNative(std::istream& stream) {
  nighthawk::internal::SimpleStatistic proto;
  std::string tmp(std::istreambuf_iterator<char>(stream), {});
  if (!proto.ParseFromString(tmp)) {
    ENVOY_LOG(error, "Failed to read back SimpleStatistic data.");
    return absl::Status(absl::StatusCode::kInternal, "Failed to read back SimpleStatistic data");
  }
  id_ = proto.id();
  count_ = proto.count();
  min_ = proto.min();
  max_ = proto.max();
  sum_x_ = proto.sum_x();
  sum_x2_ = proto.sum_x_2();
  return absl::OkStatus();
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

  combined->min_ = std::min(a.min(), b.min());
  combined->max_ = std::max(a.max(), b.max());
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

absl::StatusOr<std::unique_ptr<std::istream>> StreamingStatistic::serializeNative() const {
  nighthawk::internal::StreamingStatistic proto;
  proto.set_id(id());
  proto.set_count(count());
  proto.set_min(min());
  proto.set_max(max());
  proto.set_mean(mean_);
  proto.set_accumulated_variance(accumulated_variance_);

  std::string tmp;
  proto.SerializeToString(&tmp);
  auto write_stream = std::make_unique<std::stringstream>();
  *write_stream << tmp;
  return write_stream;
}

absl::Status StreamingStatistic::deserializeNative(std::istream& stream) {
  nighthawk::internal::StreamingStatistic proto;
  std::string tmp(std::istreambuf_iterator<char>(stream), {});
  if (!proto.ParseFromString(tmp)) {
    ENVOY_LOG(error, "Failed to read back StreamingStatistic data.");
    return absl::Status(absl::StatusCode::kInternal, "Failed to read back StreamingStatistic data");
  }
  id_ = proto.id();
  count_ = proto.count();
  min_ = proto.min();
  max_ = proto.max();
  mean_ = proto.mean();
  accumulated_variance_ = proto.accumulated_variance();
  return absl::OkStatus();
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

  combined->min_ = std::min(this->min(), b.min());
  combined->max_ = std::max(this->max(), b.max());
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
    ENVOY_LOG_EVERY_POW_2(warn, "Failed to record value into HdrHistogram.");
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
      setDurationFromNanos(*percentile->mutable_duration(), iter.highest_equivalent_value);
    } else {
      percentile->set_raw_value(iter.highest_equivalent_value);
    }
    percentile->set_percentile(percentiles->percentile / 100.0);
    percentile->set_count(iter.cumulative_count);
  }

  return proto;
}

absl::StatusOr<std::unique_ptr<std::istream>> HdrStatistic::serializeNative() const {
  char* data;
  if (hdr_log_encode(histogram_, &data) == 0) {
    auto write_stream = std::make_unique<std::stringstream>();
    *write_stream << absl::string_view(data, strlen(data));
    // Free the memory allocated by hrd_log_encode.
    free(data);
    return write_stream;
  }
  ENVOY_LOG(error, "Failed to write HdrHistogram data.");
  return absl::Status(absl::StatusCode::kInternal, "Failed to write HdrHistogram data");
}

absl::Status HdrStatistic::deserializeNative(std::istream& stream) {
  std::string s(std::istreambuf_iterator<char>(stream), {});
  struct hdr_histogram* new_histogram = nullptr;
  // hdr_log_decode allocates memory for the new hdr histogram.
  if (hdr_log_decode(&new_histogram, const_cast<char*>(s.c_str()), s.length()) == 0) {
    // Free the memory allocated by our current hdr histogram.
    hdr_close(histogram_);
    // Swap in the new histogram.
    // NOTE: Our destructor will eventually call hdr_close on the new one.
    histogram_ = new_histogram;
    return absl::OkStatus();
  }
  ENVOY_LOG(error, "Failed to read back HdrHistogram data.");
  return absl::Status(absl::StatusCode::kInternal, "Failed to read back HdrHistogram data");
}

CircllhistStatistic::CircllhistStatistic() {
  histogram_ = hist_alloc();
  ASSERT(histogram_ != nullptr);
}

CircllhistStatistic::~CircllhistStatistic() { hist_free(histogram_); }

void CircllhistStatistic::addValue(uint64_t value) {
  hist_insert_intscale(histogram_, value, 0, 1);
  StatisticImpl::addValue(value);
}
double CircllhistStatistic::mean() const { return hist_approx_mean(histogram_); }
double CircllhistStatistic::pvariance() const { return pstdev() * pstdev(); }
double CircllhistStatistic::pstdev() const {
  return count() == 0 ? std::nan("") : hist_approx_stddev(histogram_);
}

StatisticPtr CircllhistStatistic::combine(const Statistic& statistic) const {
  auto combined = std::make_unique<CircllhistStatistic>();
  const auto& stat = dynamic_cast<const CircllhistStatistic&>(statistic);
  hist_accumulate(combined->histogram_, &this->histogram_, /*cnt=*/1);
  hist_accumulate(combined->histogram_, &stat.histogram_, /*cnt=*/1);

  combined->min_ = std::min(this->min(), stat.min());
  combined->max_ = std::max(this->max(), stat.max());
  combined->count_ = this->count() + stat.count();
  return combined;
}

StatisticPtr CircllhistStatistic::createNewInstanceOfSameType() const {
  return std::make_unique<CircllhistStatistic>();
}

nighthawk::client::Statistic CircllhistStatistic::toProto(SerializationDomain domain) const {
  nighthawk::client::Statistic proto = StatisticImpl::toProto(domain);
  if (count() == 0) {
    return proto;
  }

  // List of quantiles is based on hdr_proto_json.gold.
  const std::vector<double> quantiles{0,    0.1,   0.2,  0.3,   0.4,  0.5,   0.55,  0.6,
                                      0.65, 0.7,   0.75, 0.775, 0.8,  0.825, 0.85,  0.875,
                                      0.90, 0.925, 0.95, 0.975, 0.99, 0.995, 0.999, 1};
  std::vector<double> computed_quantiles(quantiles.size(), 0.0);
  hist_approx_quantile(histogram_, quantiles.data(), quantiles.size(), computed_quantiles.data());
  for (size_t i = 0; i < quantiles.size(); i++) {
    nighthawk::client::Percentile* percentile = proto.add_percentiles();
    if (domain == Statistic::SerializationDomain::DURATION) {
      setDurationFromNanos(*percentile->mutable_duration(),
                           static_cast<int64_t>(computed_quantiles[i]));
    } else {
      percentile->set_raw_value(computed_quantiles[i]);
    }
    percentile->set_percentile(quantiles[i]);
    percentile->set_count(hist_approx_count_below(histogram_, computed_quantiles[i]));
  }

  return proto;
}

SinkableStatistic::SinkableStatistic(Envoy::Stats::Scope& scope, absl::optional<int> worker_id)
    : Envoy::Stats::HistogramImplHelper(scope.symbolTable()), scope_(scope), worker_id_(worker_id) {
}

SinkableStatistic::~SinkableStatistic() {
  // We must explicitly free the StatName here in order to supply the
  // SymbolTable reference.
  MetricImpl::clear(scope_.symbolTable());
}

Envoy::Stats::Histogram::Unit SinkableStatistic::unit() const {
  return Envoy::Stats::Histogram::Unit::Unspecified;
}

Envoy::Stats::SymbolTable& SinkableStatistic::symbolTable() { return scope_.symbolTable(); }

SinkableHdrStatistic::SinkableHdrStatistic(Envoy::Stats::Scope& scope,
                                           absl::optional<int> worker_id)
    : SinkableStatistic(scope, worker_id) {}

void SinkableHdrStatistic::recordValue(uint64_t value) {
  HdrStatistic::addValue(value);
  // Currently in Envoy Scope implementation, deliverHistogramToSinks() will flush the histogram
  // value directly to stats Sinks.
  scope_.deliverHistogramToSinks(*this, value);
}

std::string SinkableHdrStatistic::tagExtractedName() const {
  if (worker_id().has_value()) {
    return fmt::format("{}.{}", worker_id().value(), id());
  } else {
    return id();
  }
}

SinkableCircllhistStatistic::SinkableCircllhistStatistic(Envoy::Stats::Scope& scope,
                                                         absl::optional<int> worker_id)
    : SinkableStatistic(scope, worker_id) {}

void SinkableCircllhistStatistic::recordValue(uint64_t value) {
  CircllhistStatistic::addValue(value);
  // Currently in Envoy Scope implementation, deliverHistogramToSinks() will flush the histogram
  // value directly to stats Sinks.
  scope_.deliverHistogramToSinks(*this, value);
}

std::string SinkableCircllhistStatistic::tagExtractedName() const {
  if (worker_id().has_value()) {
    return fmt::format("{}.{}", worker_id().value(), id());
  } else {
    return id();
  }
}

} // namespace Nighthawk
