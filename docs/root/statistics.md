# Nighthawk Statistics

## Background
Currently Nighthawk only outputs metrics at the end of a test run and there are
no metrics streamed during a test run. The work to stream its metrics is in
progress.


## Statistics in Nighthawk
All statistics below defined in Nighthawk are per worker.

For counter metric, Nighthawk use Envoy's Counter directly. For histogram
metric, Nighthawk wraps Envoy's Histogram into its own Statistic concept (see
[#391](https://github.com/envoyproxy/nighthawk/pull/391)).

Name | Type | Description
-----| ----- | ----------------
upstream_rq_total | Counter | Total number of requests sent from Nighthawk
http_1xx | Counter | Total number of response with code 1xx
http_2xx | Counter | Total number of response with code 2xx	
http_3xx | Counter | Total number of response with code 3xx	
http_4xx | Counter | Total number of response with code 4xx	
http_5xx | Counter | Total number of response with code 5xx	
http_xxx | Counter | Total number of response with code <100 or >=600
stream_resets | Counter | Total number of stream reset	
pool_overflow | Counter | Total number of times connection pool overflowed	
pool_connection_failure | Counter | Total number of times pool connection failed	
benchmark_http_client.latency_1xx | HdrStatistic | Latency (in Nanosecond) histogram of request with code 1xx	
benchmark_http_client.latency_2xx | HdrStatistic | Latency (in Nanosecond) histogram of request with code 2xx
benchmark_http_client.latency_3xx | HdrStatistic | Latency (in Nanosecond) histogram of request with code 3xx	
benchmark_http_client.latency_4xx | HdrStatistic | Latency (in Nanosecond) histogram of request with code 4xx
benchmark_http_client.latency_5xx | HdrStatistic | Latency (in Nanosecond) histogram of request with code 5xx	
benchmark_http_client.latency_xxx | HdrStatistic | Latency (in Nanosecond) histogram of request with code <100 or >=600
benchmark_http_client.queue_to_connect | HdrStatistic | Histogram of request connection time	(in Nanosecond)
benchmark_http_client.request_to_response | HdrStatistic | Latency (in Nanosecond) histogram include requests with stream reset or pool failure
benchmark_http_client.response_header_size | StreamingStatistic | Statistic of response header size (min, max, mean, pstdev values in bytes)
benchmark_http_client.response_body_size | StreamingStatistic | Statistic of response body size (min, max, mean, pstdev values in bytes)
sequencer.callback | HdrStatistic | Latency (in Nanosecond) histogram of unblocked requests
sequencer.blocking | HdrStatistic | Latency (in Nanosecond) histogram of blocked requests


## Envoy Metrics Model

[Envoy](https://github.com/envoyproxy/envoy) has 3 types of metrics	
- Counters: Unsigned integers (can only increase) represent how many times an
  event happens, e.g. total number of requests.	
- Gauges: Unsigned integers (can increase or decrease), e.g. number of active
  connections.	
- Histograms: Unsigned integers that will yield summarized percentile values.
  E.g. latency distributions.	

In Envoy, the stat
[Store](https://github.com/envoyproxy/envoy/blob/74530c92cfa3682b49b540fddf2aba40ac10c68e/include/envoy/stats/store.h#L29)
is a singleton and provides a simple interface by which the rest of the code can
obtain handles to
[scopes](https://github.com/envoyproxy/envoy/blob/958745d658752f90f544296d9e75030519a9fb84/include/envoy/stats/scope.h#L37),
counters, gauges, and histograms. Envoy counters and gauges are periodically
(configured at ~5 sec interval) flushed to the sinks. Note that currently
histogram values are sent directly to the sinks. A stat
[Sink](https://github.com/envoyproxy/envoy/blob/74530c92cfa3682b49b540fddf2aba40ac10c68e/include/envoy/stats/sink.h#L48)
is an interface that takes generic stat data and translates it into a
backend-specific wire format. Currently Envoy supports the TCP and UDP
[statsd](https://github.com/b/statsd_spec) protocol (implemented in
[statsd.h](https://github.com/envoyproxy/envoy/blob/main/source/extensions/stat_sinks/common/statsd/statsd.h)).
Users can create their own Sink subclass to translate Envoy metrics into
backend-specific format.

Nighthawk ships a UDP statsd sink, registered under the same names as Envoy's
sinks so the `--stats-sinks` examples work as documented:
`envoy.stat_sinks.statsd` (config `envoy.config.metrics.v3.StatsdSink`, UDP
`address` only; an IP literal or a host name resolved at startup) and `envoy.stat_sinks.dog_statsd`
(`envoy.config.metrics.v3.DogStatsdSink`, adds DogStatsD tags and optional
datagram batching via `max_bytes_per_datagram`). Metric names are prefixed with
`nighthawk` unless `prefix` is set. Counters are flushed as deltas (`|c`) every
`--stats-flush-interval`, gauges as values (`|g`), and every latency sample of
the sinkable Nighthawk statistics (`benchmark_http_client.latency_*`,
`benchmark_http_client.latency_grpc_ok`, `benchmark_stream.message_latency`) is
sent as a timing in milliseconds with microsecond precision (`|ms`). One UDP
socket is shared by the whole process. With `max_bytes_per_datagram` set
(DogStatsD sink), messages are packed newline-separated into datagrams of up to
that size: counters and gauges per flush, latency samples per recording thread,
which are sent when the batch fills, on the next flush, and when the sink is
destroyed at the end of the run. `--stats-sink-tag key:value` adds tags to
every message of the DogStatsD sink, e.g. to identify the run or pod. Per-worker metrics (the store's `cluster.<n>.` / `worker.<n>.` scopes and
the per-worker latency statistics) are named `worker.<n>.<rest>` with the plain
statsd sink; the DogStatsD sink drops the prefix and emits a `worker:<n>` tag
instead. Example:

```bash
nighthawk_client --stats-sinks '{name:"envoy.stat_sinks.dog_statsd",typed_config:{"@type":"type.googleapis.com/envoy.config.metrics.v3.DogStatsdSink",address:{socket_address:{address:"127.0.0.1",port_value:8125}},max_bytes_per_datagram:1400}}' --stats-flush-interval 1 http://localhost:8080/
```	

Envoy metrics can be defined using a macro, e.g.	
```cc
// Define Envoy stats.
#define ALL_CLUSTER_STATS(COUNTER, GAUGE, HISTOGRAM)	
  COUNTER(upstream_cx_total)	
  GAUGE(upstream_cx_active, NeverImport)	
  HISTOGRAM(upstream_cx_length, Milliseconds)
// Put these stats as members of a struct.
struct ClusterStats {	
  ALL_CLUSTER_STATS(GENERATE_COUNTER_STRUCT, GENERATE_GAUGE_STRUCT, GENERATE_HISTOGRAM_STRUCT)	
};
// Instantiate the above struct using a Stats::Pool.
ClusterStats stats{
  ALL_CLUSTER_STATS(POOL_COUNTER(...), POOL_GAUGE(...), POOL_HISTOGRAM(...))};

// Stats can be updated in the code:
stats.upstream_cx_total_.inc();
stats.upstream_cx_active_.set(...);
stats.upstream_cx_length_.recordValue(...);
```	

## Envoy Metrics Limitation	
Currently Envoy metrics don't support key-value map. As a result, for metrics to
be broken down by certain dimensions, we need to define a separate metric for
each dimension. For example, currently Nighthawk defines 
[separate counters](https://github.com/envoyproxy/nighthawk/blob/main/source/client/benchmark_client_impl.h#L35-L40)
to monitor the number of responses with corresponding response code.

## Envoy Metrics Flush	
Envoy uses a flush timer to periodically flush metrics into stat sinks
([here](https://github.com/envoyproxy/envoy/blob/74530c92cfa3682b49b540fddf2aba40ac10c68e/source/server/server.cc#L479-L480))
at a configured interval (default to 5 sec). For every metric flush, Envoy will
call the function
[flushMetricsToSinks()](https://github.com/envoyproxy/envoy/blob/74530c92cfa3682b49b540fddf2aba40ac10c68e/source/server/server.cc#L175)
to create a metric snapshot from Envoy stat store and flush the snapshot to all
sinks through `sink->flush(snapshot)`.	


## Metrics Export in Nighthawk	
Currently a single Nighthawk can run with multiple workers. In the future,
Nighthawk will be extended to be able to run multiple instances together. Since
each Nighthawk worker sends requests independently, we decided to export per
worker level metrics since it provides several advantages over global level
metrics (aggregated across all workers).
- Per worker level metrics provide information about the performance of each
  worker which will be hidden by the global level metrics.	
- Keep the workers independent which makes it easier/efficient to scale up to
  multiple Nighthawks with large numbers of workers. (The work to scale up to
  multiple Nighthawks is still under development).

Envoy metrics can be defined at per worker level using
[Scope](https://github.com/envoyproxy/envoy/blob/e9c2c8c4a0141c9634316e8283f98f412d0dd207/include/envoy/stats/scope.h#L35)
( e.g. `cluster.<worker_id>.total_request_sent`). The dynamic portions of
metric (e.g. `worker_id`) can be embedded into the metric name. A
[TagSpecifier](https://github.com/envoyproxy/envoy/blob/7a652daf35d7d4a6a6bad5a010fe65947ee4411a/api/envoy/config/metrics/v3/stats.proto#L182)
can be specified in the bootstrap configuration, which will transform dynamic
portions into tags. When per worker level metrics are exported from Nighthawk,
multiple per worker level metrics can be converted into a single metric with a
`worker_id` label in the stat Sink if the corresponding backend metric supports
key-value map.	

## Reference	
- [Nighthawk: architecture and key
  concepts](https://github.com/envoyproxy/nighthawk/blob/main/docs/root/overview.md)	
- [Envoy Stats
  System](https://github.com/envoyproxy/envoy/blob/main/source/docs/stats.md)	
- [Envoy Stats blog](https://blog.envoyproxy.io/envoy-stats-b65c7f363342)
