# See https://github.com/envoyproxy/envoy/blob/main/bazel/README.md#disabling-extensions for details on how this system works.
EXTENSIONS = {
    "envoy.filters.http.router": "//source/extensions/filters/http/router:config",
    "envoy.filters.http.fault": "//source/extensions/filters/http/fault:config",
    "envoy.filters.listener.tls_inspector": "//source/extensions/filters/listener/tls_inspector:config",
    "envoy.filters.network.http_connection_manager": "//source/extensions/filters/network/http_connection_manager:config",
    "envoy.tracers.zipkin": "//source/extensions/tracers/zipkin:config",
    "envoy.transport_sockets.raw_buffer": "//source/extensions/transport_sockets/raw_buffer:config",
    "envoy.access_loggers.file": "//source/extensions/access_loggers/file:config",
    "envoy.clusters.static": "//source/extensions/clusters/static:static_cluster_lib",
    "envoy.clusters.strict_dns": "//source/extensions/clusters/strict_dns:strict_dns_cluster_lib",
    "envoy.network.dns_resolver.cares": "//source/extensions/network/dns_resolver/cares:config",
}

DISABLED_BY_DEFAULT_EXTENSIONS = {
}

# These can be changed to ["//visibility:public"], for  downstream builds which
# need to directly reference Envoy extensions.
EXTENSION_CONFIG_VISIBILITY = ["//visibility:public"]
EXTENSION_PACKAGE_VISIBILITY = ["//visibility:public"]
CONTRIB_EXTENSION_PACKAGE_VISIBILITY = ["//:contrib_library"]

# Set this variable to true to disable alwayslink for envoy_cc_library.
LEGACY_ALWAYSLINK = 1
