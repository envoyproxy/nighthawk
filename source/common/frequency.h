#pragma once

#include <chrono>
#include <cmath>

namespace nighthawk {

class Frequency {
public:
  explicit constexpr Frequency(uint64_t hertz)
      : hertz_(hertz), interval_(hertz == 0 ? std::nan("") : 1.0 / hertz) {}
  uint64_t value() const { return hertz_; }
  const std::chrono::duration<double> interval() const { return interval_; }

private:
  const uint64_t hertz_;
  const std::chrono::duration<double> interval_;
};

constexpr Frequency operator"" _Hz(unsigned long long hz) { return Frequency{hz}; }
constexpr Frequency operator"" _kHz(unsigned long long khz) { return Frequency{khz * 1000}; }

} // namespace nighthawk
