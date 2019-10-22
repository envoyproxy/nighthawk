licenses(["notice"])  # Apache 2

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

api_cc_py_proto_library(
    name = "base",
    srcs = [
        "options.proto",
        "output.proto",
        "service.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@envoy_api//envoy/api/v2/auth:pkg",
        "@envoy_api//envoy/api/v2/cluster:pkg",
        "@envoy_api//envoy/api/v2/core:pkg",
    ],
)

cc_grpc_library(
    name = "grpc_service_lib",
    srcs = [
        ":base",
    ],
    grpc_only = True,
    proto_only = False,
    use_external = False,
    visibility = ["//visibility:public"],
    well_known_protos = True,
    deps = [
        ":base_cc_proto",
    ],
)
