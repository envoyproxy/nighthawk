Version history
---------------

0.3 (TBD)
=========================

### Notable (breaking) changes

- In `service.proto` a change was made to allow both `Output` and `error_detail` to co-exist at the same time.
- Both `nighthawk_client` and `nighthawk_service` will indicate execution failure (respectively through exit code or grpc reply) when connection errors and/or status code errors are observed by default.


### Changelist

- Introducing termination predicates (https://github.com/envoyproxy/nighthawk/pull/167)

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