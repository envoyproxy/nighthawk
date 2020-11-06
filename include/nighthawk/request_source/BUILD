load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_basic_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_basic_cc_library(
    name = "request_source_plugin_config_factory_lib",
    hdrs = [
        "request_source_plugin_config_factory.h",
    ],
    include_prefix = "nighthawk/request_source",
    deps = [
        "//api/request_source:request_source_plugin_cc_proto",
        "//include/nighthawk/common:request_source_lib",
        "@envoy//include/envoy/common:base_includes",
        "@envoy//include/envoy/config:typed_config_interface",
        "@envoy//source/common/api:api_lib_with_external_headers",
    ],
)
