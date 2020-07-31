# See Envoy's bazel/README.md for details on how this system works.
EXTENSIONS = {
    "envoy.filters.http.router": "//source/extensions/filters/http/router:config",
    "envoy.access_loggers.file": "//source/extensions/access_loggers/file:config",
    "envoy.filters.http.fault": "//source/extensions/filters/http/fault:config",
    "envoy.filters.network.direct_response": "//source/extensions/filters/network/direct_response:config",
    "envoy.filters.network.echo": "//source/extensions/filters/network/echo:config",
    "envoy.filters.network.http_connection_manager": "//source/extensions/filters/network/http_connection_manager:config",
    "envoy.stat_sinks.statsd": "//source/extensions/stat_sinks/statsd:config",
    "envoy.tracers.zipkin": "//source/extensions/tracers/zipkin:config",
    "envoy.transport_sockets.raw_buffer": "//source/extensions/transport_sockets/raw_buffer:config",
    "envoy.upstreams.http.http": "//source/extensions/upstreams/http/http:config",
    "envoy.upstreams.http.tcp": "//source/extensions/upstreams/http/tcp:config",
}
