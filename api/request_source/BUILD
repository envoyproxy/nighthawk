load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

licenses(["notice"])  # Apache 2

api_cc_py_proto_library(
    name = "service",
    srcs = [
        "service.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@envoy_api//envoy/api/v2/auth:pkg",
        "@envoy_api//envoy/api/v2/cluster:pkg",
        "@envoy_api//envoy/api/v2/core:pkg",
    ],
)

api_cc_py_proto_library(
    name = "request_source_plugin",
    srcs = [
        "request_source_plugin.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@envoy_api//envoy/config/core/v3:pkg",
        "@nighthawk//api/client:base",
    ],
)

cc_grpc_library(
    name = "grpc_request_source_service_lib",
    srcs = [
        ":service",
    ],
    grpc_only = True,
    proto_only = False,
    use_external = False,
    visibility = ["//visibility:public"],
    well_known_protos = True,
    deps = [
        ":service_cc_proto",
    ],
)
