#include "common/stopwatch_impl.h"

namespace Nighthawk {

void StopwatchImpl::reset() {}
void StopwatchImpl::start() {}
void StopwatchImpl::stop() {}
std::chrono::nanoseconds StopwatchImpl::elapsed() const {
  return time_source_.monotonicTime() - start_;
}

} // namespace Nighthawk
