load("@envoy//bazel:envoy_build_system.bzl", "envoy_package")
load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

licenses(["notice"])  # Apache 2

envoy_package()

api_cc_py_proto_library(
    name = "log_response_headers_proto",
    srcs = [
        "log_response_headers.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [],
)
