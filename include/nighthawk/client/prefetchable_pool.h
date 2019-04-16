#pragma once

#include "envoy/common/pure.h"

namespace Nighthawk {
namespace Client {

/**
 * Interface that adds connection prefetching capability to the pool.
 */
class PrefetchablePool {
public:
  virtual ~PrefetchablePool() = default;

  /**
   * prefetchConnections will open up the maximum number of allowed connections.
   */
  virtual void prefetchConnections() PURE;
};

} // namespace Client
} // namespace Nighthawk