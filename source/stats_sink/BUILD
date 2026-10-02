load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "envoy_stats_sink_adapter",
    srcs = [
        "envoy_stats_sink_adapter.cc",
    ],
    hdrs = [
        "envoy_stats_sink_adapter.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//api/stats_sink:envoy_stats_sink_adapter_proto_cc_proto",
        "//include/nighthawk/common:base_includes",
        "//source/common:nighthawk_common_lib",
        "@envoy//envoy/registry",
        "@envoy//envoy/stats:stats_interface",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/config:well_known_names",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
        "@envoy//source/server:configuration_lib_with_external_headers",
    ],
)
