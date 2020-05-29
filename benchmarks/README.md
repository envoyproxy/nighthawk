# WIP - Benchmarking testsuite

The NH benchmark test suite builds on top Nighthawk's integration test framework, and
can be used to scaffold tests and obtain latency reports as well as flamegraphs.

## Immediate goals

The goal is te be able to:
- run the suite against arbitrary Envoy revisions
- persist profile dumps, flamegraphs, and latency numbers per test
- run the nighthawk tools via docker
- offer stock tests, but also allow scaffolding consumer-specific tests

## Example: scavaging a selection of stock tests.

This scripts shows how to use the benchmarking suite.
It will run a selection of an example [benchmarks](test_benchmark.py)
scavenged from `/benchmarks`, which injects Envoy between the 
benchmark client and test server.

```bash
git clone https://github.com/oschaaf/nighthawk.git benchmark-test
cd benchmark-test
bazel build //benchmarks:benchmarks

# Specify the ip address family we'll be using. [v4only|v6only|all]
export ENVOY_IP_TEST_VERSIONS=v4only
# Explicit tmpdir for OSX docker, to make sure we'll use a volume that works when
export TMPDIR="$(pwd)/benchmarks/tmp"
# Nighthawk tools will be sourced from this docker image
export NH_NH_DOCKER_IMAGE="envoyproxy/nighthawk-dev:latest"
# Envoy docker image that we'll use to inject the Envoy proxy
export ENVOY_DOCKER_IMAGE_TO_TEST="envoyproxy/envoy-dev:74290ef76a76fbbf50f072dc33438791f93f68c7"

# run all tests starting with test_http_h1_small in benchmarks/
bazel-bin/benchmarks/benchmarks --log-cli-level=info -vvvv -k test_http_h1_small benchmarks/
```

# TODOs

- Copy out the artifacts and push those to a gcp bucket. Current status:
   - cpu profiles are dumped to tmp per test (named according to the test). ideally we'd
     also dump flamegraph svg's
   - raw json is send to the output on stderr. ideally we'd persist in fortio format,
     raw yaml/json, and human readable output.
- A UI -- though we may be able to get by with just a uri structure conventioned around the envoy
  sha. e.g. http://perf-ci-host/gcpsync/[envoy-sha]-[timestamp]/ to link CI, and directory index the 
  artifacts.
- Generally tidy up, possibly some refactoring
- Consider offering a prebuild version of the test suite itself in a docker image.

# FUTURE

- Allow scavenging a separate repo for tests. Currently locally sourcing tests
  by specifying one or more directories is facilitated.
- profiling / flamegraphing via perf/bcc tools; include the proxy-wasm flamegraphing research r&d
- Allow injection of other proxies: nginx, haproxy
- Allow using alt clients, like Fortio & wrk2
- An app that integrates fortios UI, pprof's web UI
- Have a mode where nighthawk_test_server provides high-res control timings in its 
  access logs
