#pragma once

#include <pthread.h>

#include <thread>

#include "nighthawk/common/platform_util.h"

namespace Nighthawk {

using namespace std::chrono_literals;

class PlatformUtilImpl : public PlatformUtil {
public:
  void yieldCurrentThread() const override { pthread_yield(); }
  void sleep(std::chrono::microseconds duration) const override {
    std::this_thread::sleep_for(duration); // NO_CHECK_FORMAT(real_time)
  };

  uint32_t determineCpuCoresWithAffinity() const override {
    // TODO(oschaaf): mull over what to do w/regard to hyperthreading.
    const pthread_t thread = pthread_self();
    cpu_set_t cpuset;
    int i;

    CPU_ZERO(&cpuset);
    i = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (i == 0) {
      return CPU_COUNT(&cpuset);
    }
    return 0;
  }
};

} // namespace Nighthawk