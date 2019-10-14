# Profiling Envoy

## Install pprof

From https://github.com/google/pprof#building-pprof

```bash
go get -u github.com/google/pprof
```

## Nighthawk's scripted benchmark

Nighthawk comes with a [small framework and experimental benchmark suite](/benchmarks/) that
will write `.prof` files to `/tmp/`:

```bash
bazel test --cache_test_results=no --compilation_mode=opt --cxxopt=-g --cxxopt=-ggdb3 //benchmarks:*
```

Note that it is possible to override Nighthawk's Envoy dependency
to link against a local version, by adding a line to `.bazelrc`:

```
build --override_repository envoy=/path/to/local/envoy/
```

Note that doing so affects both `nighthawk_client` and `nighthawk_test_server`. 

### Visualizations: the pprof web UI

After Nighthawk finishes and the server is stopped, you should have `/tmp/envoy.prof`.
`pprof` comes with a webserver which you can start as follows:

```bash
pprof -http=localhost:8888 /tmp/envoy-test_http_h1_maxrps_no_client_side_queueing_IpVersion.IPV4.prof
```

The interface served at localhost:8888 gives you various means to help with analysing the collected profile, including a flame-chart.

## Manual profiling

### Envoy build

See [building Envoy with Bazel](https://github.com/envoyproxy/envoy/tree/master/bazel#building-envoy-with-bazel).

Envoy’s static build is set up for profiling and can be build with:

```
bazel build //source/exe:envoy-static
```

More context: https://github.com/envoyproxy/envoy/blob/master/bazel/PPROF.md

### Nighthawk build

See [building Nighthawk](https://github.com/envoyproxy/nighthawk#nighthawk).

```
bazel build -c opt //:nighthawk
```

### Envoy configuration

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

For some simple complete configuration examples, see [here](test/integration/configurations).

### Run Envoy

```bash
/path/to/envoy-repo/bazel-bin/envoy-static --config-path /path/to/envoy-config.yaml
```

### Enable CPU profiling

CPU profiling [can be set via Envoy’s admin interface](https://www.envoyproxy.io/docs/envoy/latest/operations/admin#post--cpuprofiler).
For example:

```bash
curl -X POST http://your-envoy-instance:admin-port/cpuprofiler?enable=y
```

### Run Nighthawk

For example:

```bash
/path/to/nighthawk-repo/bazel-bin/nighthawk_client --concurrency 5 --rps 10000 --duration 30 http://envoy-cluster-host:envoy-cluster-port
```
