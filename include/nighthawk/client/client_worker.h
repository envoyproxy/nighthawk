#pragma once

#include <memory>

#include "envoy/common/pure.h"
#include "envoy/stats/store.h"

#include "nighthawk/client/benchmark_client.h"
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
   * @return const Envoy::Stats::Store& the statistics store associated the benchmark client.
   */
  virtual Envoy::Stats::Store& store() const PURE;

  /**
   * @return bool True iff the worker ran and completed successfully.
   */
  virtual bool success() const PURE;
};

using ClientWorkerPtr = std::unique_ptr<ClientWorker>;

} // namespace Client
} // namespace Nighthawk
