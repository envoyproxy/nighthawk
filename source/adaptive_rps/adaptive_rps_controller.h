#pragma once

#include "api/adaptive_rps/adaptive_rps.pb.h"
#include "api/client/service.grpc.pb.h"
#include "envoy/common/time.h"

namespace Nighthawk {
namespace AdaptiveRps {

namespace {

class RealTimeSource : public Envoy::TimeSource {
  Envoy::SystemTime systemTime() override { return std::chrono::system_clock::now(); }
  Envoy::MonotonicTime monotonicTime() override { return std::chrono::steady_clock::now(); }
};

} // namespace

nighthawk::adaptive_rps::AdaptiveRpsSessionOutput PerformAdaptiveRpsSession(
    nighthawk::client::NighthawkService::Stub* nighthawk_service_stub,
    const nighthawk::adaptive_rps::AdaptiveRpsSessionSpec& spec,
    std::ostream& diagnostic_ostream,
    std::unique_ptr<Envoy::TimeSource> time_source = std::make_unique<RealTimeSource>());

} // namespace AdaptiveRps
} // namespace Nighthawk
