load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

licenses(["notice"])  # Apache 2

api_cc_py_proto_library(
    name = "adaptive_load_proto",
    srcs = [
        "adaptive_load.proto",
        "benchmark_result.proto",
        "input_variable_setter_impl.proto",
        "metric_spec.proto",
        "metrics_plugin_impl.proto",
        "scoring_function_impl.proto",
        "step_controller_impl.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//api/client:base",
        "@envoy_api//envoy/config/core/v3:pkg",
    ],
)
