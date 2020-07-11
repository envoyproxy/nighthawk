#pragma once

#include "envoy/common/time.h"

#include "api/adaptive_load/adaptive_load.pb.h"
#include "api/client/service.grpc.pb.h"

namespace Nighthawk {
namespace AdaptiveLoad {

// Performs an adaptive load session defined by |spec| using the Nighthawk Service at
// |nighthawk_service_stub|. The adaptive load session consists of the Adjusting Stage and the
// Testing Stage. Adjusting Stage: Runs a series of short benchmarks, checks metrics according to
// MetricSpecs, and adjusts load up or down based on the result; returns an error if convergence is
// not detected before the deadline in the spec. Load adjustments and convergence detection are
// computed by a StepController plugin. Metric values are obtained through MetricsPlugins. Testing
// Stage: When the optimal load is found, runs one long benchmark to validate it. If
// |diagnostic_ostream| is non-null, progress messages will be written to it. |time_source| should
// be nullptr except in tests.
nighthawk::adaptive_load::AdaptiveLoadSessionOutput
PerformAdaptiveLoadSession(nighthawk::client::NighthawkService::Stub* nighthawk_service_stub,
                           const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec,
                           std::ostream& diagnostic_ostream,
                           Envoy::TimeSource* time_source) noexcept;

} // namespace AdaptiveLoad
} // namespace Nighthawk
