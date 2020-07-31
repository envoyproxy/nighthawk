# See https://github.com/envoyproxy/envoy/blob/master/bazel/README.md for details on how this system works.
EXTENSIONS = {
    "envoy.filters.http.router": "//source/extensions/filters/http/router:config",
    "envoy.filters.http.fault": "//source/extensions/filters/http/fault:config",
    "envoy.filters.listener.tls_inspector": "//source/extensions/filters/listener/tls_inspector:config",
    "envoy.filters.network.http_connection_manager": "//source/extensions/filters/network/http_connection_manager:config",
    "envoy.tracers.zipkin": "//source/extensions/tracers/zipkin:config",
    "envoy.transport_sockets.raw_buffer": "//source/extensions/transport_sockets/raw_buffer:config",
}
