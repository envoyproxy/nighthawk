#pragma once

#include "api/adaptive_rps/adaptive_rps.pb.h"
#include "api/client/service.grpc.pb.h"
#include "envoy/common/common/utility.h"
#include "envoy/common/time.h"

namespace Nighthawk {
namespace AdaptiveRps {

nighthawk::adaptive_rps::AdaptiveRpsSessionOutput PerformAdaptiveRpsSession(
    nighthawk::client::NighthawkService::Stub* nighthawk_service_stub,
    const nighthawk::adaptive_rps::AdaptiveRpsSessionSpec& spec,
    std::unique_ptr<Envoy::TimeSource> time_source = std::make_unique<Envoy::RealTimeSource>());

} // namespace AdaptiveRps
} // namespace Nighthawk
