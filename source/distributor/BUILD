load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "nighthawk_distributor_client_impl",
    srcs = [
        "nighthawk_distributor_client_impl.cc",
    ],
    hdrs = [
        "nighthawk_distributor_client_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/distributor:nighthawk_distributor_client",
    ],
)

envoy_cc_library(
    name = "grpc_service_lib",
    srcs = [
        "service_impl.cc",
    ],
    hdrs = [
        "service_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//api/distributor:distributor_grpc_lib",
        "//source/common:nighthawk_service_client_impl",
        "//source/sink:nighthawk_sink_client_impl",
        "@com_github_grpc_grpc//:grpc++",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
    ],
)
