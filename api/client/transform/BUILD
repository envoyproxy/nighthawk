licenses(["notice"])  # Apache 2

load("@envoy//bazel:envoy_build_system.bzl", "envoy_package")
load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

envoy_package()

api_cc_py_proto_library(
    name = "transform",
    srcs = [
        "fortio.proto",
    ],
    visibility = ["//visibility:public"],
)
