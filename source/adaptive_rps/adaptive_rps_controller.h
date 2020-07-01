#pragma once

#include "api/adaptive_rps/adaptive_rps.pb.h"
#include "api/client/service.grpc.pb.h"

namespace Nighthawk {
namespace AdaptiveRps {

nighthawk::adaptive_rps::AdaptiveRpsSessionOutput
PerformAdaptiveRpsSession(nighthawk::client::NighthawkService::Stub* nighthawk_service_stub,
                          const nighthawk::adaptive_rps::AdaptiveRpsSessionSpec& spec);

} // namespace AdaptiveRps
} // namespace Nighthawk
