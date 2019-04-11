#pragma once

#include <functional>
#include <memory>

#include "envoy/runtime/runtime.h"
#include "envoy/stats/store.h"

#include "nighthawk/common/statistic.h"

namespace Nighthawk {
namespace Client {

class BenchmarkClient {
public:
  virtual ~BenchmarkClient() = default;

  /**
   * Initialize will be called on the worker thread after it has started.
   * @param runtime to be used during initialization.
   */
  virtual void initialize(Envoy::Runtime::Loader& runtime) PURE;

  /**
   * Terminate will be called on the worker thread before it ends.
   */
  virtual void terminate() PURE;

  /**
   * Turns latency measurement on or off.
   *
   * @param measure_latencies true iff latencies should be measured.
   */
  virtual void setMeasureLatencies(bool measure_latencies) PURE;

  /**
   * Gets the statistics, keyed by id.
   * @return StatisticPtrMap A map of Statistics keyed by id.
   */
  virtual StatisticPtrMap statistics() const PURE;

  /**
   * Tries to start a request. In open-loop mode this MUST always return true.
   *
   * @param caller_completion_callback The callback the client must call back upon completion of a
   * successfully started request.
   *
   * @return true if the request could be started, otherwise the request could not be started, for
   * example due to resource limits
   */
  virtual bool tryStartOne(std::function<void()> caller_completion_callback) PURE;

  /**
   * @return const Envoy::Stats::Store& the statistics store associated the benchmark client.
   */
  virtual Envoy::Stats::Store& store() const PURE;

  /**
   * Determines if latency measurement is on.
   *
   * @return bool indicating if latency measurement is enabled.
   */
  virtual bool measureLatencies() const PURE;
};

typedef std::unique_ptr<BenchmarkClient> BenchmarkClientPtr;

} // namespace Client
} // namespace Nighthawk