licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)

envoy_package()

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@envoy_api//bazel:api_build_system.bzl", "api_proto_library")

api_proto_library(
    name = "base",
    srcs = [
        "options.proto",
        "output.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@envoy_api//envoy/api/v2/core:base_export",
    ],
)

# TODO(oschaaf): This is a bit of a hack, we conjure the expected _base_cc_only ourselves here
# because it doesn't get generated for us. Also see https://github.com/grpc/grpc/issues/14145
proto_library(
    name = "_base_cc_only",
    srcs = [
        "options.proto",
        "output.proto",
        "service.proto",
    ],
    deps = [
        "@com_github_gogo_protobuf//:gogo_proto",
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:descriptor_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:empty_proto",
        "@com_google_protobuf//:struct_proto",
        "@com_google_protobuf//:timestamp_proto",
        "@com_google_protobuf//:wrappers_proto",
        "@com_lyft_protoc_gen_validate//validate:validate_proto",
        "@envoy_api//envoy/api/v2/core:base_export",
        "@googleapis//:http_api_protos_proto",
        "@googleapis//:rpc_status_protos_lib",
    ],
)

cc_grpc_library(
    name = "grpc_service_lib",
    srcs = [
        "service.proto",
    ],
    proto_only = False,
    use_external = True,
    visibility = ["//visibility:public"],
    well_known_protos = True,
    deps = [
        ":base_cc",
    ],
)
