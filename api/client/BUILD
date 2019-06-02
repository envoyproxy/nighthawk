licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load("@envoy_api//bazel:api_build_system.bzl", "api_proto_library")

envoy_package()

load("@build_stack_rules_proto//cpp:cpp_grpc_library.bzl", "cpp_grpc_library")

api_proto_library(
    name = "base",
    srcs = [
        "options.proto",
        "output.proto",
        "service.proto",
    ],
    has_services = 1,
    visibility = ["//visibility:public"],
    deps = [
        "@envoy_api//envoy/api/v2/core:base_export",
    ],
)

#api_proto_library(
#    name = "service_proto",
#    srcs = [
#        "service.proto",
#    ],
#    deps = [
#        ":base",
#    ],
#)

#cpp_grpc_library(
#    name = "cpp_service_proto",
#    has_services = True,
#    include_imports = False,
#    transitive = True,
#    deps = [
#        ":base",
#        ":service_proto",
#    ],
#)
