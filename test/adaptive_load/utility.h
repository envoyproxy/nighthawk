#pragma once

#include "api/client/output.pb.h"

namespace Nighthawk {
namespace AdaptiveLoad {

// Creates a Nighthawk output proto containing minimal counters and statistics for analysis by the
// nighthawk.builtin MetricsPlugin:
//   - 1024 RPS attempted
//   - 10 second duration attempted
//   - 10240 requests attempted
//   - 2560 requests performed (counter upstream_rq_total) (0.25 send-rate)
//   - 320 requests returned 2xx (counter benchmark.http_2xx) (0.125 success-rate)
//   - Latency stats (benchmark_http_client.request_to_response):
//     - 400ns min
//     - 500ns mean
//     - 600ns max
//     - 11ns pstdev
nighthawk::client::Output MakeStandardNighthawkOutput();

} // namespace AdaptiveLoad
} // namespace Nighthawk
