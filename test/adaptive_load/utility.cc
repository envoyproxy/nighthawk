#include "test/adaptive_load/utility.h"

#include "envoy/filesystem/filesystem.h"

#include "absl/time/time.h"
#include <google/protobuf/util/time_util.h>

#include "external/envoy/source/common/protobuf/protobuf.h"

#include "absl/strings/string_view.h"

namespace Nighthawk {

namespace {

using ::Envoy::Protobuf::util::TimeUtil;

void SetCounterValue(nighthawk::client::Counter* counter, absl::string_view name, int value) {
  counter->set_name(std::string(name));
  counter->set_value(value);
}

void SetStatisticValues(nighthawk::client::Statistic* statistic, absl::string_view id, long min_ns,
                        long mean_ns, long max_ns, long pstdev_ns) {
  statistic->set_id(std::string(id));
  *statistic->mutable_min() = TimeUtil::NanosecondsToDuration(min_ns);
  *statistic->mutable_mean() = TimeUtil::NanosecondsToDuration(mean_ns);
  *statistic->mutable_max() = TimeUtil::NanosecondsToDuration(max_ns);
  *statistic->mutable_pstdev() = TimeUtil::NanosecondsToDuration(pstdev_ns);
}

} // namespace

nighthawk::client::Output MakeSimpleNighthawkOutput(const SimpleNighthawkOutputSpec& spec) {
  nighthawk::client::Output output;
  output.mutable_options()->mutable_concurrency()->set_value(spec.concurrency);
  output.mutable_options()->mutable_requests_per_second()->set_value(spec.requests_per_second);
  nighthawk::client::Result* result = output.mutable_results()->Add();
  result->mutable_execution_duration()->set_seconds(spec.actual_duration_seconds);
  result->set_name("global");
  SetCounterValue(result->mutable_counters()->Add(), "upstream_rq_total", spec.upstream_rq_total);
  SetCounterValue(result->mutable_counters()->Add(), "benchmark.http_2xx", spec.response_count_2xx);
  SetStatisticValues(result->mutable_statistics()->Add(),
                     "benchmark_http_client.request_to_response", spec.min_ns, spec.mean_ns,
                     spec.max_ns, spec.pstdev_ns);
  return output;
}

Envoy::SystemTime FakeIncrementingMonotonicTimeSource::systemTime() {
  Envoy::SystemTime epoch;
  return epoch;
}

Envoy::MonotonicTime FakeIncrementingMonotonicTimeSource::monotonicTime() {
  ++unix_time_;
  Envoy::MonotonicTime epoch;
  return epoch + std::chrono::seconds(unix_time_);
}

} // namespace Nighthawk
