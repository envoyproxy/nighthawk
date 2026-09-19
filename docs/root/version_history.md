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

- `--envoy-stats-sinks` configures stats sinks implemented as Envoy stats sink plugins, resolved through `Envoy::Server::Configuration::StatsSinkFactory` rather than Nighthawk's own `NighthawkStatsSinkFactory`, so any Envoy sink linked into the binary can be used without writing a Nighthawk specific factory. It can be combined with `--stats-sinks`. Envoy's UDP statsd and DogStatsD sinks are linked in. Sinks that hold a gRPC client, such as the OpenTelemetry and metrics service sinks, are not usable yet: Envoy binds the client to the dispatcher of the thread that creates the sink, while Nighthawk flushes stats from its own flush worker thread, so flushing one trips `ASSERT(isThreadSafe())`.
- Nighthawk's latency statistics are now recorded into an Envoy store histogram as well as their own HdrHistogram/Circllhist, so they appear in `MetricSnapshot::histograms()`. Stats sinks that only read the snapshot on flush, such as the OpenTelemetry and metrics service sinks, previously received nothing for them: Nighthawk delivered samples exclusively through `deliverHistogramToSinks()`, which those sinks implement as a no-op. Sinks reading `onHistogramComplete()` are unaffected, and the emitted metric names are unchanged, since the mirror is created in the worker's `cluster.<n>.` scope. Nighthawk's own output still comes from the statistic itself and keeps its nanosecond resolution.
- `--request-body-file` sends a file's bytes verbatim as the request body (binary safe; no Content-Type is set). The `RequestOptions.request_body` (bytes) field carries it over the gRPC service API. Mutually exclusive with `--request-body-size`.
- `OptionsImpl::toCommandLineOptions()` now always emits `request_options.request_body_size`; it was only set when at least one `--request-header` was configured, so the size was lost on the gRPC service path otherwise.
- The Envoy exception on the tunneling startup path is logged instead of printed to stdout, keeping stdout reserved for the formatted output.
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