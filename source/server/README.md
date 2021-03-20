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

It is possible to
[enable additional envoy extension](https://github.com/envoyproxy/envoy/blob/main/source/extensions/extensions_build_config.bzl) by adding them [here](../../extensions_build_config.bzl) before the build.
By default, Nighthawk's test server is set up with the minimum extension set needed
for it to operate as documented.


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
            - name: envoy.filters.network.http_connection_manager
              typed_config:
                "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
                generate_request_id: false
                codec_type: AUTO
                stat_prefix: ingress_http
                route_config:
                  name: local_route
                  virtual_hosts:
                    - name: service
                      domains:
                        - "*"
                http_filters:
                  - name: dynamic-delay
                    typed_config:
                      "@type": type.googleapis.com/nighthawk.server.ResponseOptions
                      static_delay: 0.5s
                  - name: test-server # before envoy.router because order matters!
                    typed_config:
                      "@type": type.googleapis.com/nighthawk.server.ResponseOptions
                      response_body_size: 10
                      v3_response_headers:
                        - { header: { key: "foo", value: "bar" } }
                        - {
                            header: { key: "foo", value: "bar2" },
                            append: true,
                          }
                        - { header: { key: "x-nh", value: "1" } }
                  - name: envoy.filters.http.router
                    typed_config:
                      "@type": type.googleapis.com/envoy.extensions.filters.http.router.v3.Router
                      dynamic_stats: false
admin:
  access_log_path: /tmp/envoy.log
  address:
    socket_address:
      address: 0.0.0.0
      port_value: 8081
```

## Response Options config

The [ResponseOptions proto](/api/server/response_options.proto) is shared by
the `Test Server` and `Dynamic Delay` filter extensions. Each filter will
interpret the parts that are relevant to it. This allows specifying what
a response should look like in a single message, which can be done at request
time via the optional `x-nighthawk-test-server-config` request-header.

### Test Server

- `response_body_size` - number of 'a' characters repeated in the response body.
- `response_headers` - list of headers to add to response. If `append` is set to
  `true`, then the header is appended.
- `echo_request_headers` - if set to `true`, then append the dump of request headers to the response
  body.

The response options above could be used to test and debug proxy or server configuration, for example, to verify request headers that are added by intermediate proxy:

```bash
$ curl -6 -v [::1]:8080/nighthawk

*   Trying ::1:8080...
* TCP_NODELAY set
* Connected to ::1 (::1) port 8080 (#0)
> GET /nighthawk
> Host: [::1]:8080
> User-Agent: curl/7.68.0
> Accept: */*
>
* Mark bundle as not supporting multiuse
< HTTP/1.1 200 OK
< content-length: 254
< content-type: text/plain
< foo: bar
< foo: bar2
< x-nh: 1
< date: Wed, 03 Jun 2020 13:34:41 GMT
< server: envoy
< x-service: nighthawk_cluster
< via: 1.1 envoy
<
aaaaaaaaaa
Request Headers:
':authority', '[::1]:8080'
':path', '/nighthawk'
':method', 'GET'
':scheme', 'https'
'user-agent', 'curl/7.68.0'
'accept', '*/*'
'x-forwarded-proto', 'http'
'via', '1.1 google'
'x-forwarded-for', '::1,::1'
* Connection #0 to host ::1 left intact
```

This example shows that intermediate proxy has added `x-forwarded-proto` and
`x-forwarded-for` request headers.

### Dynamic Delay

The Dynamic Delay interprets the `oneof_delay_options` part in the [ResponseOptions proto](/api/server/response_options.proto). If specified, it can be used to:

- Configure a static delay via `static_delay`.
- Configure a delay which linearly increase as the number of active requests grows, representing a simplified model of an overloaded server, via `concurrency_based_linear_delay`.

All delays have a millisecond-level granularity.

At the time of writing this, there is a [known issue](https://github.com/envoyproxy/nighthawk/issues/392) with merging configuration provided via
request headers into the statically configured configuration. The current recommendation is to
use either static, or dynamic configuration (delivered per request header), but not both at the
same time.

## Running the test server

```
# If you already have Envoy running, you might need to set --base-id to allow the test-server to start.
➜ /bazel-bin/nighthawk/source/server/server --config-path /path/to/test-server.yaml

# Verify the test server with a curl command similar to:
➜ curl -H "x-nighthawk-test-server-config: {response_body_size:20, static_delay: \"0s\"}" -vv 127.0.0.1:10000
```

```bash
➜ bazel-bin/nighthawk_test_server --help
```

```
USAGE:

bazel-bin/nighthawk_test_server  [--enable-core-dump] [--socket-mode
<string>] [--socket-path <string>]
[--disable-extensions <string>]
[--cpuset-threads]
[--enable-mutex-tracing]
[--disable-hot-restart] [--mode
<string>] [--parent-shutdown-time-s
<uint32_t>] [--drain-strategy <string>]
[--drain-time-s <uint32_t>]
[--file-flush-interval-msec <uint32_t>]
[--service-zone <string>]
[--service-node <string>]
[--service-cluster <string>]
[--hot-restart-version]
[--restart-epoch <uint32_t>]
[--log-path <string>]
[--enable-fine-grain-logging]
[--log-format-escaped] [--log-format
<string>] [--component-log-level
<string>] [-l <string>]
[--local-address-ip-version <string>]
[--admin-address-path <string>]
[--ignore-unknown-dynamic-fields]
[--reject-unknown-dynamic-fields]
[--allow-unknown-static-fields]
[--allow-unknown-fields]
[--bootstrap-version <string>]
[--config-yaml <string>] [-c <string>]
[--concurrency <uint32_t>]
[--base-id-path <string>]
[--use-dynamic-base-id] [--base-id
<uint32_t>] [--] [--version] [-h]


Where:

--enable-core-dump
Enable core dumps

--socket-mode <string>
Socket file permission

--socket-path <string>
Path to hot restart socket file

--disable-extensions <string>
Comma-separated list of extensions to disable

--cpuset-threads
Get the default # of worker threads from cpuset size

--enable-mutex-tracing
Enable mutex contention tracing functionality

--disable-hot-restart
Disable hot restart functionality

--mode <string>
One of 'serve' (default; validate configs and then serve traffic
normally) or 'validate' (validate configs and exit).

--parent-shutdown-time-s <uint32_t>
Hot restart parent shutdown time in seconds

--drain-strategy <string>
Hot restart drain sequence behaviour, one of 'gradual' (default) or
'immediate'.

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

--enable-fine-grain-logging
Logger mode: enable file level log control(Fancy Logger)or not

--log-format-escaped
Escape c-style escape sequences in the application logs

--log-format <string>
Log message format in spdlog syntax (see
https://github.com/gabime/spdlog/wiki/3.-Custom-formatting)

Default is "[%Y-%m-%d %T.%e][%t][%l][%n] [%g:%#] %v"

--component-log-level <string>
Comma separated list of component log levels. For example
upstream:debug,config:trace

-l <string>,  --log-level <string>
Log levels: [trace][debug][info][warning
|warn][error][critical][off]

Default is [info]

--local-address-ip-version <string>
The local IP address version (v4 or v6).

--admin-address-path <string>
Admin address path

--ignore-unknown-dynamic-fields
ignore unknown fields in dynamic configuration

--reject-unknown-dynamic-fields
reject unknown fields in dynamic configuration

--allow-unknown-static-fields
allow unknown fields in static configuration

--allow-unknown-fields
allow unknown fields in static configuration (DEPRECATED)

--bootstrap-version <string>
API version to parse the bootstrap config as (e.g. 3). If unset, all
known versions will be attempted

--config-yaml <string>
Inline YAML configuration, merges with the contents of --config-path

-c <string>,  --config-path <string>
Path to configuration file

--concurrency <uint32_t>
# of worker threads to run

--base-id-path <string>
path to which the base ID is written

--use-dynamic-base-id
the server chooses a base ID dynamically. Supersedes a static base ID.
May not be used when the restart epoch is non-zero.

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
