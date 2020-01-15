# Nighthawk test server

A test-server filter which is capable of generating test responses.

## Testing

```bash
bazel test -c dbg //test/server:http_test_server_filter_integration_test
```

## Building

```bash
bazel build -c opt :nighthawk_test_server
```

## Configuring the test server


`test-server.yaml` sample content

```yaml
static_resources:
  listeners:
  # define an origin server on :10000 that always returns "lorem ipsum..."
  - address:
      socket_address:
        address: 0.0.0.0
        port_value: 10000
    filter_chains:
    - filters:
      - name: envoy.http_connection_manager
        config:
          generate_request_id: false
          codec_type: auto
          stat_prefix: ingress_http
          route_config:
            name: local_route
            virtual_hosts:
            - name: service
              domains:
              - "*"
          http_filters:
          - name: envoy.fault
            config:
              max_active_faults: 100
              delay:
                header_delay: {}
                percentage:
                  numerator: 100
          - name: test-server   # before envoy.router because order matters!
            config:
              response_body_size: 10
              response_headers:
              - { header: { key: "foo", value: "bar"} }
              - { header: { key: "foo", value: "bar2"}, append: true }
              - { header: { key: "x-nh", value: "1"}}
          - name: envoy.router
            config:
              dynamic_stats: false
admin:
  access_log_path: /tmp/envoy.log
  address:
    socket_address:
      address: 0.0.0.0
      port_value: 8081
```

## Running the test server


```
# If you already have Envoy running, you might need to set --base-id to allow the test-server to start.
➜ /bazel-bin/nighthawk/source/server/server --config-path /path/to/test-server-server.yaml

# Verify the test server with a curl command similar to:
➜ curl -H "x-envoy-fault-delay-request: 1000" -H "x-nighthawk-test-server-config: {response_body_size:20}"  -vv 127.0.0.1:10000
```

```bash
➜ bazel-bin/nighthawk_test_server --help
```

```
USAGE:

bazel-bin/nighthawk_test_server  [--disable-extensions <string>]
[--use-fake-symbol-table <bool>]
[--cpuset-threads]
[--enable-mutex-tracing]
[--disable-hot-restart]
[--max-obj-name-len <uint64_t>]
[--max-stats <uint64_t>] [--mode
<string>] [--parent-shutdown-time-s
<uint32_t>] [--drain-time-s <uint32_t>]
[--file-flush-interval-msec <uint32_t>]
[--service-zone <string>]
[--service-node <string>]
[--service-cluster <string>]
[--hot-restart-version]
[--restart-epoch <uint32_t>]
[--log-path <string>]
[--log-format-escaped] [--log-format
<string>] [--component-log-level
<string>] [-l <string>]
[--local-address-ip-version <string>]
[--admin-address-path <string>]
[--reject-unknown-dynamic-fields]
[--allow-unknown-static-fields]
[--allow-unknown-fields] [--config-yaml
<string>] [-c <string>] [--concurrency
<uint32_t>] [--base-id <uint32_t>] [--]
[--version] [-h]


Where:

--disable-extensions <string>
Comma-separated list of extensions to disable

--use-fake-symbol-table <bool>
Use fake symbol table implementation

--cpuset-threads
Get the default # of worker threads from cpuset size

--enable-mutex-tracing
Enable mutex contention tracing functionality

--disable-hot-restart
Disable hot restart functionality

--max-obj-name-len <uint64_t>
Deprecated and unused; please do not specify.

--max-stats <uint64_t>
Deprecated and unused; please do not specify.

--mode <string>
One of 'serve' (default; validate configs and then serve traffic
normally) or 'validate' (validate configs and exit).

--parent-shutdown-time-s <uint32_t>
Hot restart parent shutdown time in seconds

--drain-time-s <uint32_t>
Hot restart and LDS removal drain time in seconds

--file-flush-interval-msec <uint32_t>
Interval for log flushing in msec

--service-zone <string>
Zone name

--service-node <string>
Node name

--service-cluster <string>
Cluster name

--hot-restart-version
hot restart compatibility version

--restart-epoch <uint32_t>
hot restart epoch #

--log-path <string>
Path to logfile

--log-format-escaped
Escape c-style escape sequences in the application logs

--log-format <string>
Log message format in spdlog syntax (see
https://github.com/gabime/spdlog/wiki/3.-Custom-formatting)

Default is "[%Y-%m-%d %T.%e][%t][%l][%n] %v"

--component-log-level <string>
Comma separated list of component log levels. For example
upstream:debug,config:trace

-l <string>,  --log-level <string>
Log levels:
[trace][debug][info][warning][error][critical][off]

Default is [info]

--local-address-ip-version <string>
The local IP address version (v4 or v6).

--admin-address-path <string>
Admin address path

--reject-unknown-dynamic-fields
reject unknown fields in dynamic configuration

--allow-unknown-static-fields
allow unknown fields in static configuration

--allow-unknown-fields
allow unknown fields in static configuration (DEPRECATED)

--config-yaml <string>
Inline YAML configuration, merges with the contents of --config-path

-c <string>,  --config-path <string>
Path to configuration file

--concurrency <uint32_t>
# of worker threads to run

--base-id <uint32_t>
base ID so that multiple envoys can run on the same host if needed

--,  --ignore_rest
Ignores the rest of the labeled arguments following this flag.

--version
Displays version information and exits.

-h,  --help
Displays usage information and exits.


envoy
```
