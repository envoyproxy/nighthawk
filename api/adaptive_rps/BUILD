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
        "step_controller.proto",
        "step_controller_impl.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@nighthawk//api/client:base",
    ],
)

# api_cc_py_proto_library(
#     name = "adaptive_rps",
#     srcs = [
#         "adaptive_rps.proto",
#     ],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":benchmark_result_proto",
#         ":metric_spec_proto",
#         ":metrics_plugin_proto",
#         ":step_controller_proto",
#         "//google/protobuf:duration",
#         "//third_party/nighthawk/api/client:base",
#         "@envoy_api//envoy/config/core/v3:pkg",
#         "@envoy_api//envoy/extensions/transport_sockets/tls/v3:pkg",
#     ],
# )

# api_cc_py_proto_library(
#     name = "benchmark_result",
#     srcs = [
#         "benchmark_result.proto",
#     ],
#     visibility = ["//visibility:public"],
#     deps = [
#         ":metric_spec_proto",
#         "//third_party/nighthawk/api/client:base",
#     ],
# )

# api_cc_py_proto_library(
#     name = "custom_metric_evaluator",
#     srcs = [
#         "custom_metric_evaluator.proto",
#     ],
#     visibility = ["//visibility:public"],
#     deps = [
#         "//google/protobuf:any",
#     ],
# )

# api_cc_py_proto_library(
#     name = "custom_metric_evaluator_impl",
#     srcs = [
#         "custom_metric_evaluator_impl.proto",
#     ],
#     visibility = ["//visibility:public"],
#     deps = [
#     ],
# )

# api_cc_py_proto_library(
#     name = "metrics_plugin",
#     srcs = [
#         "metrics_plugin.proto",
#     ],
#     visibility = ["//visibility:public"],
#     deps = [
#     ],
# )

# api_cc_py_proto_library(
#     name = "metric_spec",
#     srcs = [
#         "metric_spec.proto",
#     ],
#     visibility = ["//visibility:public"],
#     deps = [
#     ],
# )

# api_cc_py_proto_library(
#     name = "step_controller",
#     srcs = [
#         "step_controller.proto",
#     ],
#     visibility = ["//visibility:public"],
#     deps = [
#     ],
# )

# api_cc_py_proto_library(
#     name = "step_controller_impl",
#     srcs = [
#         "step_controller_impl.proto",
#     ],
#     visibility = ["//visibility:public"],
#     deps = [
#     ],
# )
