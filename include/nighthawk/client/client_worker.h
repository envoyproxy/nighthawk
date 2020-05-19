#pragma once

#include <memory>

#include "envoy/common/pure.h"
#include "envoy/stats/store.h"

#include "nighthawk/client/benchmark_client.h"
#include "nighthawk/common/phase.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/worker.h"

namespace Nighthawk {
namespace Client {

/**
 * Interface for a threaded benchmark client worker.
 */
class ClientWorker : virtual public Worker {
public:
  /**
   * Gets the statistics, keyed by id.
   * @return StatisticPtrMap A map of Statistics keyed by id.
   */
  virtual StatisticPtrMap statistics() const PURE;

  /**
   * @return const std::map<std::string, uint64_t>& The worker-specific counter values.
   * Gets filled when the worker has completed its task, empty before that.
   */
  virtual const std::map<std::string, uint64_t>& threadLocalCounterValues() PURE;

  /**
   * @return const Phase& associated to this worker.
   */
  virtual const Phase& phase() const PURE;
};

using ClientWorkerPtr = std::unique_ptr<ClientWorker>;

} // namespace Client
} // namespace Nighthawk
