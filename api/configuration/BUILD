load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

licenses(["notice"])  # Apache 2

api_cc_py_proto_library(
    name = "cluster_config_manager",
    srcs = [
        "cluster_config_manager.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@envoy_api//envoy/config/core/v3:pkg",
    ],
)

api_cc_py_proto_library(
    name = "endpoints_config_manager",
    srcs = [
        "endpoints_config_manager.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@envoy_api//envoy/config/core/v3:pkg",
    ],
)
