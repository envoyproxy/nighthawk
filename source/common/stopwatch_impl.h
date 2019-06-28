#pragma once

#include <vector>

#include "envoy/common/time.h"

#include "nighthawk/common/stopwatch.h"

#include "common/common/assert.h"

namespace Nighthawk {

class StopwatchImpl : public Stopwatch {
public:
  StopwatchImpl(Envoy::TimeSource& time_source) : time_source_(time_source) {}

  void reset() override;
  void start() override;
  void stop() override;
  std::chrono::nanoseconds elapsed() const override;

private:
  Envoy::TimeSource& time_source_;
  Envoy::MonotonicTime start_;
};

} // namespace Nighthawk
