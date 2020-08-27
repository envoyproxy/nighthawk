#pragma once

#include "envoy/common/time.h"

#include "external/envoy/source/common/common/statusor.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/client/service.grpc.pb.h"

namespace Nighthawk {

/**
 * Performs an adaptive load session consisting of the Adjusting Stage and the
 * Testing Stage. Adjusting Stage: Runs a series of short benchmarks, checks metrics according to
 * MetricSpecs, and adjusts load up or down based on the result; returns an error if convergence is
 * not detected before the deadline in the spec. Load adjustments and convergence detection are
 * computed by a StepController plugin. Metric values are obtained through MetricsPlugins. Testing
 * Stage: When the optimal load is found, runs one long benchmark to validate it.
 *
 * @param nighthawk_service_stub A Nighthawk Service gRPC stub.
 * @param spec A proto that defines all aspects of the adaptive load session, including metrics,
 * threshold, duration of adjusting stage benchmarks, and underlying Nighthawk traffic parameters.
 * @param time_source An abstraction of the system clock. Normally, just construct an
 * Envoy::Event::RealTimeSystem and pass it. NO_CHECK_FORMAT(real_time). If calling from an
 * Envoy-based process, there may be an existing TimeSource or TimeSystem to use. If calling
 * from a test, pass a fake TimeSource.
 *
 * @return StatusOr<AdaptiveLoadSessionOutput> A proto logging the result of all traffic attempted
 * and all corresponding metric values and scores, or an overall error status if the session failed.
 */
absl::StatusOr<nighthawk::adaptive_load::AdaptiveLoadSessionOutput> PerformAdaptiveLoadSession(
    nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
    const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec, Envoy::TimeSource& time_source);

} // namespace Nighthawk
