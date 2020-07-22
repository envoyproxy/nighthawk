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
// Stage: When the optimal load is found, runs one long benchmark to validate it. Progress messages
// are written to |diagnostic_ostream| such as std::cerr or a std::ostringstream. |time_source| can
// be an Envoy::Event::RealTimeSystem constructed from scratch. NO_CHECK_FORMAT(real_time)
nighthawk::adaptive_load::AdaptiveLoadSessionOutput PerformAdaptiveLoadSession(
    nighthawk::client::NighthawkService::StubInterface* nighthawk_service_stub,
    const nighthawk::adaptive_load::AdaptiveLoadSessionSpec& spec, std::ostream& diagnostic_ostream,
    Envoy::TimeSource& time_source);

} // namespace AdaptiveLoad
} // namespace Nighthawk
