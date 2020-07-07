load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

licenses(["notice"])  # Apache 2

api_cc_py_proto_library(
    name = "base",
    srcs = [
        "adaptive_rps.proto",
        "benchmark_result.proto",
        "custom_metric_evaluator.proto",
        "custom_metric_evaluator_impl.proto",
        "metric_spec.proto",
        "metrics_plugin.proto",
        "metrics_plugin_impl.proto",
        "step_controller.proto",
        "step_controller_impl.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@nighthawk//api/client:base",
    ],
)
