#pragma once

#include <list>
#include <random>

#include "envoy/common/time.h"

#include "nighthawk/common/rate_limiter.h"

#include "external/envoy/source/common/common/logger.h"

#include "source/common/frequency.h"

#include "absl/random/random.h"
#include "absl/random/zipf_distribution.h"
#include "absl/types/optional.h"

namespace Nighthawk {

/**
 * Rate limiter base class, which implements some shared functionality for derivations that
 * compute acquireable counts based on elapsed time. Rate limiters that apply filters,offsets
 * or otherwise wrap another rate limiter should derive from ForwardingRateLimiterImpl instead.
 */
class RateLimiterBaseImpl : public RateLimiter {
public:
  RateLimiterBaseImpl(Envoy::TimeSource& time_source) : time_source_(time_source){};
  Envoy::TimeSource& timeSource() override { return time_source_; }
  std::chrono::nanoseconds elapsed() override {
    // TODO(oschaaf): consider adding an explicit start() call to the interface.
    const auto now = time_source_.monotonicTime();
    if (start_time_ == absl::nullopt) {
      first_acquisition_time_ = time_source_.systemTime();
      start_time_ = now;
    }
    return now - start_time_.value();
  }

  absl::optional<Envoy::SystemTime> firstAcquisitionTime() const override {
    return first_acquisition_time_;
  }

private:
  Envoy::TimeSource& time_source_;
  absl::optional<Envoy::MonotonicTime> start_time_;
  absl::optional<Envoy::SystemTime> first_acquisition_time_;
};

/**
 * Simple rate limiter that will allow acquiring at a linear pace.
 * The average rate is computed over a timeframe that starts at
 * the first call to tryAcquireOne().
 */
class LinearRateLimiter : public RateLimiterBaseImpl,
                          public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  LinearRateLimiter(Envoy::TimeSource& time_source, const Frequency frequency);
  bool tryAcquireOne() override;
  void releaseOne() override;

protected:
  int64_t acquireable_count_{0};
  uint64_t acquired_count_{0};
  const Frequency frequency_;
};

/**
 * A rate limiter which linearly ramps up to the desired frequency over the specified ramp_time.
 */
class LinearRampingRateLimiterImpl : public RateLimiterBaseImpl,
                                     public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  LinearRampingRateLimiterImpl(Envoy::TimeSource& time_source,
                               const std::chrono::nanoseconds ramp_time, const Frequency frequency);
  bool tryAcquireOne() override;
  void releaseOne() override;

private:
  int64_t acquireable_count_{0};
  uint64_t acquired_count_{0};
  const std::chrono::nanoseconds ramp_time_;
  const Frequency frequency_;
};

/**
 * Base for a rate limiter which wraps another rate limiter, and forwards
 * some calls.
 */
class ForwardingRateLimiterImpl : public RateLimiter {
public:
  ForwardingRateLimiterImpl(RateLimiterPtr&& rate_limiter)
      : rate_limiter_(std::move(rate_limiter)) {}
  Envoy::TimeSource& timeSource() override { return rate_limiter_->timeSource(); }
  std::chrono::nanoseconds elapsed() override { return rate_limiter_->elapsed(); }
  absl::optional<Envoy::SystemTime> firstAcquisitionTime() const override {
    return rate_limiter_->firstAcquisitionTime();
  }

protected:
  const RateLimiterPtr rate_limiter_;
};

/**
 * BurstingRatelimiter can be wrapped around another rate limiter. It has two modes:
 * 1. First it will be accumulating acquisitions by forwarding calls to the wrapped
 *    rate limiter, until the accumulated acquisitions equals the specified burst size.
 * 2. Release mode. In this mode, BatchingRateLimiter is in control and will be handling
 *    acquisition calls (returning true and substracting from the accumulated total until
 *    nothing is left, after which mode 1 will be entered again).
 */
class BurstingRateLimiter : public ForwardingRateLimiterImpl,
                            public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  BurstingRateLimiter(RateLimiterPtr&& rate_limiter, const uint64_t burst_size);
  bool tryAcquireOne() override;
  void releaseOne() override;

private:
  const uint64_t burst_size_;
  uint64_t accumulated_{0};
  bool releasing_{};
  absl::optional<bool> previously_releasing_; // Solely used for sanity checking.
};

/**
 * Rate limiter that only starts forwarding calls to the wrapped rate limiter
 * after it is time to start.
 */
class ScheduledStartingRateLimiter : public ForwardingRateLimiterImpl,
                                     public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  /**
   * @param rate_limiter The rate limiter that will be forwarded to once it is time to start.
   * @param scheduled_starting_time The starting time
   */
  ScheduledStartingRateLimiter(RateLimiterPtr&& rate_limiter,
                               const Envoy::MonotonicTime scheduled_starting_time);
  bool tryAcquireOne() override;
  void releaseOne() override;

private:
  const Envoy::MonotonicTime scheduled_starting_time_;
  bool aquisition_attempted_{false};
};

/**
 * The consuming rate limiter will hold off opening up until the initial point in time plus the
 * offset obtained via the delegate have transpired.
 * We use an unsigned duration here to ensure only future points in time will be yielded.
 */
using RateLimiterDelegate = std::function<const std::chrono::duration<uint64_t, std::nano>()>;

/**
 * Wraps a rate limiter, and allows plugging in a delegate which will be queried to offset the
 * timing of the underlying rate limiter.
 */
class DelegatingRateLimiterImpl : public ForwardingRateLimiterImpl,
                                  public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  DelegatingRateLimiterImpl(RateLimiterPtr&& rate_limiter,
                            RateLimiterDelegate random_distribution_generator);
  bool tryAcquireOne() override;
  void releaseOne() override;

protected:
  const RateLimiterDelegate random_distribution_generator_;

private:
  std::list<Envoy::MonotonicTime> distributed_timings_;
  // Used to enforce that releaseOne() is always paired with a successfull tryAcquireOne().
  bool sanity_check_pending_release_{true};
};

class UniformRandomDistributionSamplerImpl : public DiscreteNumericDistributionSampler {
public:
  UniformRandomDistributionSamplerImpl(const uint64_t upper_bound)
      : distribution_(0, upper_bound) {}
  uint64_t getValue() override { return distribution_(generator_); }
  uint64_t min() const override { return distribution_.min(); }
  uint64_t max() const override { return distribution_.max(); }

private:
  std::default_random_engine generator_;
  std::uniform_int_distribution<uint64_t> distribution_;
};

// Allows adding uniformly distributed random timing offsets to an underlying rate limiter.
class DistributionSamplingRateLimiterImpl : public DelegatingRateLimiterImpl {
public:
  DistributionSamplingRateLimiterImpl(DiscreteNumericDistributionSamplerPtr&& provider,
                                      RateLimiterPtr&& rate_limiter);

private:
  DiscreteNumericDistributionSamplerPtr provider_;
};

/**
 * Callback used to indicate if a rate limiter release should be supressed or not.
 */
using RateLimiterFilter = std::function<bool()>;

// Wraps a rate limiter, and allows plugging in a delegate which will be queried to apply a
// filter to acquisitions.
class FilteringRateLimiterImpl : public ForwardingRateLimiterImpl,
                                 public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  FilteringRateLimiterImpl(RateLimiterPtr&& rate_limiter, RateLimiterFilter filter);
  bool tryAcquireOne() override;
  void releaseOne() override { rate_limiter_->releaseOne(); }

protected:
  const RateLimiterFilter filter_;
};

/**
 * Takes a probabilistic approach to suppressing an arbitrary wrapped rate limiter.
 */
class GraduallyOpeningRateLimiterFilter : public FilteringRateLimiterImpl {
public:
  /**
   * @param ramp_time Time that should elapse between moving from complete
   * suppression to completely opening the wrapped rate limiter.
   * @param provider Distrete numeric distribution sampler. To achieve a
   * reasonable precision, the min value of this distribution MUST equal 1. The max value MUST equal
   * 1000000. Configuring otherwise will result in a NighthawkException. Using a uniform
   * distribution will yield an approximately linear ramp from completely closed to completely
   * opened.
   * @param rate_limiter The rate limiter that will be wrapped and responsible
   * for generating the base acquisition pacing that we will operate on.
   */
  GraduallyOpeningRateLimiterFilter(const std::chrono::nanoseconds ramp_time,
                                    DiscreteNumericDistributionSamplerPtr&& provider,
                                    RateLimiterPtr&& rate_limiter);

private:
  DiscreteNumericDistributionSamplerPtr provider_;
  const std::chrono::nanoseconds ramp_time_;
};

/**
 * Thin wrapper around absl::zipf_distribution that will pull zeroes and ones from the distribution
 * with the intent to probabilistically suppress the wrapped rate limiter.
 * This may need further consideration, because it will shoot holes in the pacing, lowering the
 * actual achieved frequency.
 */
class ZipfRateLimiterImpl : public FilteringRateLimiterImpl {
public:
  enum class ZipfBehavior { ZIPF_PSEUDO_RANDOM, ZIPF_RANDOM };
  /**
   * From the absl header associated to the zipf distribution:
   * The parameters v and q determine the skew of the distribution.
   * zipf_distribution produces random integer-values in the range [0, k],
   * distributed according to the discrete probability function: P(x) = (v + x) ^ -q.
   * Preconditions: v > 0, q > 1, configuring otherwise throws a NighthawkException.
   */
  ZipfRateLimiterImpl(RateLimiterPtr&& rate_limiter, double q = 2.0, double v = 1.0,
                      ZipfBehavior behavior = ZipfBehavior::ZIPF_RANDOM);

private:
  absl::zipf_distribution<uint64_t> dist_;
  absl::InsecureBitGen g_;
  std::mt19937_64 mt_;
  ZipfBehavior behavior_;
};

} // namespace Nighthawk