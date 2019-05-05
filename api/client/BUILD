licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)

envoy_package()

load("@envoy_api//bazel:api_build_system.bzl", "api_proto_library_internal")

api_proto_library_internal(
    name = "benchmark_options",
    srcs = [
        "options.proto",
        "output.proto",
    ],
    visibility = ["//visibility:public"],
    deps = ["@envoy_api//envoy/api/v2/core:base_export"],
)
