load("@envoy//api/bazel:api_build_system.bzl", "api_proto_library_internal")

licenses(["notice"])  # Apache 2

api_proto_library_internal(
    name = "benchmark_options",
    srcs = [
        "options.proto",
        "output.proto",
    ],
    visibility = ["//visibility:public"],
    deps = ["@envoy_api//envoy/api/v2/core:base_export"],
)
