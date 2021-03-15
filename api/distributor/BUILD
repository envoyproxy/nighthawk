load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

licenses(["notice"])  # Apache 2

api_cc_py_proto_library(
    name = "distributor",
    srcs = [
        "distributor.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//api/client:base",
        "//api/sink",
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
    well_known_protos = True,
    deps = [
        ":distributor_cc_proto",
    ],
)
