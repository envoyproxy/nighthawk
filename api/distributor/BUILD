load("@envoy//bazel:envoy_build_system.bzl", "envoy_package")
load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")
load("@grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

licenses(["notice"])  # Apache 2

envoy_package()

api_cc_py_proto_library(
    name = "distributor",
    srcs = [
        "distributor.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//api/client:base",
        "@envoy_api//envoy/config/core/v3:pkg",
    ],
)

cc_grpc_library(
    name = "distributor_grpc_lib",
    srcs = [
        ":distributor",
    ],
    generate_mocks = True,
    grpc_only = True,
    proto_only = False,
    use_external = False,
    visibility = ["//visibility:public"],
    deps = [
        ":distributor_cc_proto",
    ],
)
