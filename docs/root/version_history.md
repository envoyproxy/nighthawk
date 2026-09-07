Version history
---------------

0.3 (TBD)
=========================

### Notable (breaking) changes

- In `service.proto` a change was made to allow both `Output` and `error_detail` to co-exist at the same time.
- Both `nighthawk_client` and `nighthawk_service` will indicate execution failure (respectively through exit code or grpc reply) when connection errors and/or status code errors are observed by default.
- The simple warmup we performed earlier has been removed, to eliminate counter pollution. This will be restored
  when configuration of phases lands in a next release. For those who need the old behavior, `--simple-warmup`
  can be configured to opt-in to the old-style behavior again.

### Changelist

- `--request-body-file` sends a file's bytes verbatim as the request body (binary safe; no Content-Type is set). The `RequestOptions.request_body` (bytes) field carries it over the gRPC service API. Mutually exclusive with `--request-body-size`.
- `--grpc-mode unary` issues gRPC unary calls: implies HTTP/2 and POST, sets `content-type: application/grpc` and `te: trailers`, frames the `--request-body-file` bytes as a gRPC message, and scores responses on `grpc-status`. New counters `benchmark.grpc_error` and `benchmark.grpc_status.<code>` (`.missing` when absent), new statistic `benchmark_http_client.latency_grpc_ok`; a failed RPC is not counted as `benchmark.http_2xx`.
- `--grpc-mode bidi-stream` adds gRPC bidirectional streaming load: `--streams` long-lived streams are opened to the URI path and the `--request-body-file` message is sent on them at an aggregate `--rps` message rate on an absolute schedule; echoes are correlated per stream and measured in `benchmark_stream.message_latency`. Sends that would exceed `--max-inflight-per-stream` are dropped and counted in `benchmark.stream_deferred`; streams are half-closed and drained for `--stream-drain-duration` at the end; streams still open when the window ends are counted in `benchmark.stream_drain_incomplete` and their unanswered messages in `benchmark.stream_inflight_lost`. See the `benchmark.stream_*` counters.
- Introducing termination predicates (https://github.com/envoyproxy/nighthawk/pull/167) and https://github.com/envoyproxy/nighthawk/pull/176

0.2 (July 16, 2019)
=========================

- Nighthawk as a service: (https://github.com/envoyproxy/nighthawk/issues/22)
- Add option to control how request pacing is maintained (https://github.com/envoyproxy/nighthawk/issues/80)
- Add python orchestration for integration testing: https://github.com/envoyproxy/nighthawk/issues/50
- Benchmark client configuration options:
  - Connection-pool configuration https://github.com/envoyproxy/nighthawk/issues/45
  - Allow control of TLS ciphers and settings https://github.com/envoyproxy/nighthawk/issues/32
  - Measure time spend waiting on a full connection queue: https://github.com/envoyproxy/nighthawk/pull/97

0.1 (May 6, 2019)
=========================

Initial release.