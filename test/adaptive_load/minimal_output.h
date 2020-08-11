#pragma once

#include "envoy/common/time.h"

#include "api/client/output.pb.h"

namespace Nighthawk {

/**
 * Minimal description for unit tests to construct a fake nighthawk::client::Output proto using
 * MakeSimpleNighthawkOutput().
 */
struct SimpleNighthawkOutputSpec {
  // String that is either "auto" or a decimal worker count.
  std::string concurrency;
  // requests_per_second passed in by the caller of the Nighthawk Service.
  int requests_per_second;
  // Actual duration, to be stored in the Result in the output.
  int actual_duration_seconds;
  // Counter that records all requests Nighthawk attempted to send.
  int upstream_rq_total;
  // Counter that records Nighthawk sending a request and receiving a 2xx response.
  int response_count_2xx;
  // Minimum latency statistic.
  long min_ns;
  // Mean latency statistic.
  long mean_ns;
  // Max latency statistic.
  long max_ns;
  // pstdev latency statistic.
  long pstdev_ns;
};

/**
 * Creates a Nighthawk output proto containing minimal counters and statistics for analysis by the
 * nighthawk.builtin MetricsPlugin. The output corresponds to Nighthawk output for a single worker.
 *
 * @return nighthawk::client::Output Nighthawk benchmark output proto.
 */
nighthawk::client::Output MakeSimpleNighthawkOutput(const SimpleNighthawkOutputSpec& spec);

} // namespace Nighthawk
