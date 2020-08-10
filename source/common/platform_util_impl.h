#pragma once

#include <pthread.h>

#include <thread>

#include "nighthawk/common/platform_util.h"

namespace nighthawk {

using namespace std::chrono_literals;

class PlatformUtilImpl : public PlatformUtil {
public:
  void yieldCurrentThread() const override {
#ifdef __APPLE__
    pthread_yield_np();
#else
    pthread_yield();
#endif
  }
  void sleep(std::chrono::microseconds duration) const override {
    std::this_thread::sleep_for(duration); // NO_CHECK_FORMAT(real_time)
  };
};

} // namespace nighthawk