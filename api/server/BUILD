load("@envoy_api//bazel:api_build_system.bzl", "api_proto_library")

package(default_visibility = ["//visibility:public"])

api_proto_library(
    name = "response_options_proto",
    srcs = ["response_options.proto"],
    deps = [
        "@envoy_api//envoy/api/v2/core:base_export",
    ],
)
