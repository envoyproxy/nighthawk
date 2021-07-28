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
        "//include/nighthawk/common:nighthawk_service_client",
        "@com_github_grpc_grpc//:grpc++",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "distributed_process_impl_lib",
    srcs = [
        "distributed_process_impl.cc",
    ],
    hdrs = [
        "distributed_process_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//source/client:options_impl_lib",
        "//source/distributor:nighthawk_distributor_client_impl",
        "//source/sink:nighthawk_sink_client_impl",
        "@envoy//source/common/common:random_generator_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/protobuf:utility_lib_with_external_headers",
    ],
)
