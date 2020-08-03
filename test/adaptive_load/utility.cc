#include "test/adaptive_load/utility.h"

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

nighthawk::client::Output MakeStandardNighthawkOutput() {
  nighthawk::client::Output output;
  output.mutable_options()->mutable_concurrency()->set_value("1");
  output.mutable_options()->mutable_requests_per_second()->set_value(1024);
  nighthawk::client::Result* result = output.mutable_results()->Add();
  result->mutable_execution_duration()->set_seconds(10);
  result->set_name("global");
  // 1/4 of requests were successfully sent.
  SetCounterValue(result->mutable_counters()->Add(), "upstream_rq_total", 2560);
  // 1/8 of successfully sent requests returned 2xx.
  SetCounterValue(result->mutable_counters()->Add(), "benchmark.http_2xx", 320);
  SetStatisticValues(result->mutable_statistics()->Add(),
                     "benchmark_http_client.request_to_response", /*min_ns=*/400, /*mean_ns=*/500,
                     /*max_ns=*/600, /*pstdev_ns=*/11);
  return output;
}

} // namespace Nighthawk
