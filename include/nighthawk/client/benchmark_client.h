#pragma once

#include <functional>
#include <memory>

#include "envoy/http/header_map.h"
#include "envoy/runtime/runtime.h"
#include "envoy/stats/store.h"

#include "nighthawk/common/operation_callback.h"
#include "nighthawk/common/statistic.h"
#include "nighthawk/common/uri.h"

namespace Nighthawk {
namespace Client {

using CompletionCallback = OperationCallback;

class BenchmarkClient {
public:
  virtual ~BenchmarkClient() = default;

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
  virtual bool tryStartOne(CompletionCallback caller_completion_callback) PURE;

  /**
   * @return const Envoy::Stats::Store& the statistics store associated the benchmark client.
   */
  virtual Envoy::Stats::Store& store() const PURE;

  virtual void prefetchPoolConnections() PURE;

  /**
   * Determines if latency measurement is on.
   *
   * @return bool indicating if latency measurement is enabled.
   */
  virtual bool measureLatencies() const PURE;

  /**
   * Sets the request method to use when sending request.
   * @param request_method to set.
   */
  virtual void setRequestMethod(envoy::api::v2::core::RequestMethod request_method) PURE;

  /**
   * Sets request header named 'key' to the specified value.
   *
   * @param key Name of the request header to set.
   * @param value Value to set the request header to.
   */
  virtual void setRequestHeader(absl::string_view key, absl::string_view value) PURE;

  /**
   * @brief Set the Request Body Size
   *
   * @param request_body_size number of bytes to send
   */
  virtual void setRequestBodySize(uint32_t request_body_size) PURE;

  /**
   * @return const HeaderMap& the request headers.
   */
  virtual const Envoy::Http::HeaderMap& requestHeaders() const PURE;
};

using BenchmarkClientPtr = std::unique_ptr<BenchmarkClient>;

} // namespace Client
} // namespace Nighthawk