# Nighthawk Statistics

## Background
Currently Nighthawk only outputs metrics at the end of a test run. There are no metrics streamed during a test run. In order to collect all the useful Nighthawk metrics, we plan to instrument Nighthawk with the functionality to stream its metrics.


## Statistics in BenchMarkClient
Per worker Statistics are defined in [benchmark_client_impl.h](https://github.com/envoyproxy/nighthawk/blob/master/source/client/benchmark_client_impl.h)

Name | Type | Description
-----| ----- | ----------------
total_req_sent | Counter | Total number of requests sent from Nighthawk
http_xxx | Counter | Total number of response with code xxx
stream_resets | Counter | Total number of sream reset
pool_overflow | Counter | Total number of times connection pool overflowed
pool_connection_failure | Counter | Total number of times pool connection failed
latency_on_success_req | Histogram | Latency (in Microseconds) histogram of successful request with code 2xx
latency_on_error_req | Histogram | Latency (in Microseconds) histogram of error request with code other than 2xx

## Envoy Metrics Model
Since Nighthawk is built on top of [Envoy](https://github.com/envoyproxy/envoy) and similar metrics have been exported from Envoy, it is natural to follow the example in Envoy. Furthermore *Envoy typed metrics are already being used in Nighthawk* ([example](https://github.com/envoyproxy/nighthawk/blob/master/source/client/benchmark_client_impl.h#L33-L46)).


Envoy has 3 types of metrics
- Counters: Unsigned integers (can only increase) represent how many times an event happens, e.g. total number of requests.
- Gauges: Unsigned integers (can increase or decrease), e.g. number of active connections.
- Histograms: Unsigned integers that will yield summarized percentile values. E.g. latency distributions.


In Envoy, the stat [Store](https://github.com/envoyproxy/envoy/blob/74530c92cfa3682b49b540fddf2aba40ac10c68e/include/envoy/stats/store.h#L29) is a singleton and provides a simple interface by which the rest of the code can obtain handles to scopes, counters, gauges, and histograms. Envoy counters and gauges are periodically (configured at ~5 sec interval) flushed to the sinks. Note that currently histogram values are sent directly to the sinks. A stat [Sink](https://github.com/envoyproxy/envoy/blob/74530c92cfa3682b49b540fddf2aba40ac10c68e/include/envoy/stats/sink.h#L48) is an interface that takes generic stat data and translates it into a backend-specific wire format. Currently Envoy supports the TCP and UDP [statsd](https://github.com/b/statsd_spec) protocol (implemented in [statsd.h](https://github.com/envoyproxy/envoy/blob/master/source/extensions/stat_sinks/common/statsd/statsd.h)). Users can create their own Sink subclass to translate Envoy metrics into backend-specific format.

Envoy metrics can be defined using a macro, e.g.
```
#define ALL_CLUSTER_STATS(COUNTER, GAUGE, HISTOGRAM)
  COUNTER(upstream_cx_total)
  GAUGE(upstream_cx_active, NeverImport)
  HISTOGRAM(upstream_cx_length, Milliseconds)

struct ClusterStats {
  ALL_CLUSTER_STATS(GENERATE_COUNTER_STRUCT, GENERATE_GAUGE_STRUCT, GENERATE_HISTOGRAM_STRUCT)
};
```

## Envoy Metrics Limitation
Currently Envoy metrics don't support key-value map. As a result, for metrics to be broken down by certain dimensions, we need to define a separate metric for each dimension. For example, currently Nighthawk defines separate [counters](https://github.com/envoyproxy/nighthawk/blob/master/source/client/benchmark_client_impl.h#L35-L40) to monitor the number of responses with corresponding response code.

Due to this limitation, for the latency metric we define 2 metrics: *latency_on_success_req* (to capture latency of successful requests with 2xx) and *latency_on_error_req* (to capture latency of error requests with code other than 2xx).


## Envoy Metrics Flush
Envoy uses a flush timer to periodically flush metrics into stat sinks ([here](https://github.com/envoyproxy/envoy/blob/74530c92cfa3682b49b540fddf2aba40ac10c68e/source/server/server.cc#L479-L480)) at a configured interval (default to 5 sec). For every metric flush, Envoy will call the function [flushMetricsToSinks()](https://github.com/envoyproxy/envoy/blob/74530c92cfa3682b49b540fddf2aba40ac10c68e/source/server/server.cc#L175) to create a metric snapshot from Envoy stat store and flush the snapshot to all sinks through `sink->flush(snapshot)`.


## Metrics Export in Nighthawk
Currently a single Nighthawk can run with multiple workers. In the future, Nighthawk will be extended to be able to run multiple instances together. Since each Nighthawk worker sends requests independently, we decide to export per worker level metrics since it provides several advantages over global level metrics (aggregated across all workers). Notice that *there are existing Nighthawk metrics already defined at per worker level* ([example](https://github.com/envoyproxy/nighthawk/blob/bc72a52efdc489beaa0844b34f136e03394bd355/source/client/benchmark_client_impl.cc#L61)).
- Per worker level metrics provide information about the performance of each worker which will be hidden by the global level metrics.
- Keep the workers independent which makes it easier/efficient to scale up to multiple Nighthawks with large numbers of workers.

Metrics can be defined at per worker level using [Scope](https://github.com/envoyproxy/envoy/blob/e9c2c8c4a0141c9634316e8283f98f412d0dd207/include/envoy/stats/scope.h#L35) ( e.g. `cluster.<worker_id>.total_request_sent`). The dynamic portions of metric (e.g. `worker_id`) can be embedded into the metric name. A [TagSpecifier](https://github.com/envoyproxy/envoy/blob/7a652daf35d7d4a6a6bad5a010fe65947ee4411a/api/envoy/config/metrics/v3/stats.proto#L182) can be specified in the bootstrap configuration, which will transform dynamic portions into tags. When per worker level metrics are exported from Nighthawk, multiple per worker level metrics can be converted into a single metric with a `worker_id` label in the stat Sink if the corresponding backend metric supports key-value map.

## Reference
- [Nighthawk: architecture and key concepts](https://github.com/envoyproxy/nighthawk/blob/master/docs/root/overview.md)
- [Envoy Stats System](https://github.com/envoyproxy/envoy/blob/master/source/docs/stats.md)
- [Envoy Stats blog](https://blog.envoyproxy.io/envoy-stats-b65c7f363342)
