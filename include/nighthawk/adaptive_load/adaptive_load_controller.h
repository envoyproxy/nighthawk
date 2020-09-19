#pragma once

#include "envoy/common/pure.h"

#include "external/envoy/source/common/common/statusor.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/client/service.grpc.pb.h"

namespace Nighthawk {

/**
 * Contains the main loop of the adaptive load controller. Consults a StepController for load
 * decisions, interacts with Nighthawk Service and MetricsPlugins.
 */
class AdaptiveLoadController {
public:
  virtual ~AdaptiveLoadController() = default;
  /**
   * Performs an adaptive load session consisting of the Adjusting Stage and the
   * Testing Stage.
   *
   * Adjusting Stage: Runs a series of short benchmarks, checks metrics according to MetricSpecs,
   * and adjusts load up or down based on the result. Returns an error if convergence is not
   * detected before the deadline in the spec. Load adjustments and convergence detection are
   * computed by a StepController plugin. Metric values are obtained through MetricsPlugins.
   *
   * Testing Stage: When the optimal load is found, runs one long benchmark to validate it.
   *
   * @param nighthawk_service_stub A Nighthawk Service gRPC stub.
   * @param spec A proto that defines all aspects of the adaptive load session, including metrics,
   * threshold, duration of adjusting stage benchmarks, and underlying Nighthawk traffic parameters.
   *
   * @return StatusOr<AdaptiveLoadSessionOutput> A proto logging the result of all traffic attempted
   * and all corresponding metric values and scores, or an overall error status if the session
   * failed.
   */
  virtual absl::StatusOr<nighthawk::adaptive_load::AdaptiveLoadSessionOutput>
  PerformAdaptiveLoadSession(
      nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
      const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec) PURE;
};

} // namespace Nighthawk
