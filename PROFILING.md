# Profiling Envoy

## Install pprof

From https://github.com/google/pprof#building-pprof
go get -u github.com/google/pprof

## Envoy build

See [building Envoy with Bazel](https://github.com/envoyproxy/envoy/tree/master/bazel#building-envoy-with-bazel).

Envoy’s static build is set up for profiling and can be build with:

```
bazel build //source/exe:envoy-static
```

More context: https://github.com/envoyproxy/envoy/blob/master/bazel/PPROF.md

## Nighthawk build

See [building Nighthawk](https://github.com/envoyproxy/nighthawk#nighthawk).

```
bazel build -c opt //:nighthawk
```

There’s also some recommendations in Nighthawk’s README.md for improving accuracy and repeatability of its measurements.

## Envoy configuration

The important part is that the admin interface needs to be set up to allow one to enable / disable
Profiling via http, as well as specify a location to dump the profiling data.

``` yaml
admin:
  access_log_path: /tmp/admin_access.log
  profile_path: /tmp/envoy.prof
  address:
    socket_address: { address: $server_ip, port_value: 0 }
static_resources:
.. your configuration ..
```

Also see some simple complete configuration examples [here](test/integration/configurations).

## Run Envoy

```bash
/path/to/envoy-repo/bazel-bin/envoy-static --config-path /path/to/envoy-config.yaml
```

Enable cpu profiling through Envoy’s admin interface 

```bash
curl -X POST http://your-envoy-instance:admin-port/cpuprofiler?enable=y
```

https://www.envoyproxy.io/docs/envoy/latest/operations/admin#post--cpuprofiler

Note: there’s also [PR #160](https://github.com/envoyproxy/nighthawk/pull/160) which is an invitation for discussing if it would make sense for NH to facilitate consolidation of tests scenarios in its repo.

## Run Nighthawk

Run your test. For example Nighthawk.

```bash
/path/to/nighthawk-repo/bazel-bin/nighthawk_client --concurrency 5 --rps 10000 --duration 30 http://envoy-cluster-host:envoy-cluster-port
```

Run pprof web UI

```bash
pprof -http=localhost:8888 /tmp/envoy.prof
```

Gives you various means to help with analysing the collected profile, including a flame-chart.
Sample (on temporary VM, visualizing a profile drawn from NH’s integration tests):
http://34.90.107.89/ui/

