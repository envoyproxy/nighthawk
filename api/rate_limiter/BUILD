load("@envoy//bazel:envoy_build_system.bzl", "envoy_package")
load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

licenses(["notice"])

envoy_package()

api_cc_py_proto_library(
    name = "rate_limiter_plugin",
    srcs = [
        "linear_ramping_rate_limiter.proto",
        "stub_rate_limiter.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//api/client:base",
    ],
)
