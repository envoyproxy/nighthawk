# Nighthawk Statistics

## Statistics in BenchMarkClient
Per worker Statistics are defined in [benchmark_client_impl.h](https://github.com/envoyproxy/nighthawk/blob/master/source/client/benchmark_client_impl.h)

Name | Type | Description
-----| ----- | ----------------
total_req_sent | Counter | Total number of requests sent from Nighthawk
http_xxx | Counter | Total number of response with code xxx
stream_resets | Counter | Total number of sream reset
pool_overflow | Counter | Total number of times connection pool overflowed
pool_connection_failure | Counter | Total number of times pool connection failed
latency_on_success_req_us | HISTOGRAM | Latency (in Microseconds) histogram of successful request with code 2xx
latency_on_error_req_us | HISTOGRAM | Latency (in Microseconds) histogram of error request with code other than 2xx
