#pragma once

#include "api/adaptive_rps/adaptive_rps.pb.h"
#include "api/client/service.grpc.pb.h"
#include "envoy/common/time.h"

namespace Nighthawk {
namespace AdaptiveRps {

// Performs an adaptive RPS session specified by |spec| using the Nighthawk Service at
// |nighthawk_service_stub|. The adaptive RPS session consists of the Adjusting Stage and the
// Testing Stage. Adjusting Stage: Runs a series of short benchmarks, checks metrics according to
// MetricSpecs and adjusts RPS up or down based on the result; returns an error if convergence is
// not detected before the deadline in the spec. RPS adjustments and convergence detection are
// managed by a Step Controller. Testing Stage: When the optimal RPS is found, runs one long
// benchmark to validate the RPS. If |diagnostic_ostream| is non-null, progress messages will be
// written to it. |time_source| should be nullptr except in tests.
nighthawk::adaptive_rps::AdaptiveRpsSessionOutput
PerformAdaptiveRpsSession(nighthawk::client::NighthawkService::Stub* nighthawk_service_stub,
                          const nighthawk::adaptive_rps::AdaptiveRpsSessionSpec& spec,
                          std::ostream* diagnostic_ostream,
                          Envoy::TimeSource* time_source = nullptr);

}  // namespace AdaptiveRps
}  // namespace Nighthawk
