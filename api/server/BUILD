licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)

envoy_package()

load("@envoy_api//bazel:api_build_system.bzl", "api_proto_library")

api_proto_library(
    name = "response_options_proto",
    srcs = ["response_options.proto"],
    deps = [
        "@envoy_api//envoy/api/v2/core:base_export",
    ],
)
