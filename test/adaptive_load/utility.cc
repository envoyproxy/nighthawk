#include "test/adaptive_load/utility.h"

namespace Nighthawk {

namespace {

void SetCounterValue(nighthawk::client::Counter* counter, const std::string& name, int value) {
  counter->set_name(name);
  counter->set_value(value);
}

void SetStatisticValues(nighthawk::client::Statistic* statistic, const std::string& id, long min_ns,
                        long mean_ns, long max_ns, long pstdev_ns) {
  const long kOneBillion = 1000 * 1000 * 1000;
  statistic->set_id(id);
  statistic->mutable_min()->set_seconds(min_ns / kOneBillion);
  statistic->mutable_min()->set_nanos(min_ns % kOneBillion);
  statistic->mutable_mean()->set_seconds(mean_ns / kOneBillion);
  statistic->mutable_mean()->set_nanos(mean_ns % kOneBillion);
  statistic->mutable_max()->set_seconds(max_ns / kOneBillion);
  statistic->mutable_max()->set_nanos(max_ns % kOneBillion);
  statistic->mutable_pstdev()->set_seconds(pstdev_ns / kOneBillion);
  statistic->mutable_pstdev()->set_nanos(pstdev_ns % kOneBillion);
}

} // namespace

nighthawk::client::Output MakeStandardNighthawkOutput() {
  nighthawk::client::Output output;
  output.mutable_options()->mutable_concurrency()->set_value("1");
  output.mutable_options()->mutable_requests_per_second()->set_value(1024);
  output.mutable_options()->mutable_duration()->set_seconds(10);
  nighthawk::client::Result* result = output.mutable_results()->Add();
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
