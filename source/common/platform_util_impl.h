#pragma once

#include <pthread.h>

#include "nighthawk/common/platform_util.h"

namespace Nighthawk {

class PlatformUtilImpl : public PlatformUtil {
public:
  void yieldCurrentThread() const override { pthread_yield(); }
};

} // namespace Nighthawk