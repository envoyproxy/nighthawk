#include "common/stopwatch_impl.h"

namespace Nighthawk {

StopwatchImpl::StopwatchImpl(Envoy::TimeSource& time_source)
    : time_source_(time_source), started_at_(Envoy::MonotonicTime::min()),
      stopped_at_(Envoy::MonotonicTime::min()) {}

void StopwatchImpl::reset() {
  started_at_ = Envoy::MonotonicTime::min();
  stopped_at_ = Envoy::MonotonicTime::min();
  running_ = false;
}

void StopwatchImpl::start() {
  running_ = true;
  if (started_at_ == Envoy::MonotonicTime::min()) {
    started_at_ = time_source_.monotonicTime();
  }
}

void StopwatchImpl::stop() {
  stopped_at_ = time_source_.monotonicTime();
  running_ = false;
}

std::chrono::nanoseconds StopwatchImpl::elapsed() const {
  if (running_) {
    return time_source_.monotonicTime() - started_at_;
  } else {
    return stopped_at_ - started_at_;
  }
}

} // namespace Nighthawk
