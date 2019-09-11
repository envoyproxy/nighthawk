licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "replay_grpc_client_lib",
    srcs = ["replay_grpc_client_impl.cc"],
    hdrs = ["replay_grpc_client_impl.h"],
    repository = "@envoy",
    deps = [
        "//api/client:base_cc",
        "//api/client:grpc_service_lib",
        "@envoy//include/envoy/grpc:async_client_interface_with_external_headers",
        "@envoy//include/envoy/grpc:async_client_manager_interface_with_external_headers",
        "@envoy//include/envoy/upstream:cluster_manager_interface_with_external_headers",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/grpc:typed_async_client_lib_with_external_headers",
        "@envoy//source/common/http:header_map_lib_with_external_headers",
        "@envoy//source/common/http:headers_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "nighthawk_common_lib",
    srcs = [
        "header_source_impl.cc",
        "rate_limiter_impl.cc",
        "sequencer_impl.cc",
        "statistic_impl.cc",
        "uri_impl.cc",
        "utility.cc",
        "worker_impl.cc",
    ],
    hdrs = [
        "frequency.h",
        "header_source_impl.h",
        "platform_util_impl.h",
        "rate_limiter_impl.h",
        "sequencer_impl.h",
        "statistic_impl.h",
        "uri_impl.h",
        "utility.h",
        "worker_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":replay_grpc_client_lib",
        "//api/client:base_cc",
        "//include/nighthawk/client:client_includes",
        "//include/nighthawk/common:base_includes",
        "@dep_hdrhistogram_c//:hdrhistogram_c",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:lock_guard_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/common:thread_lib_with_external_headers",
        "@envoy//source/common/http:utility_lib_with_external_headers",
        "@envoy//source/common/network:utility_lib_with_external_headers",
        "@envoy//source/common/stats:stats_lib_with_external_headers",
        "@envoy//source/exe:envoy_common_lib_with_external_headers",
        "@envoy//source/server/config_validation:server_lib_with_external_headers",
    ],
)
