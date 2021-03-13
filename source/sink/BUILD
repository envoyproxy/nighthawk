load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "nighthawk_sink_client_impl",
    srcs = [
        "nighthawk_sink_client_impl.cc",
    ],
    hdrs = [
        "nighthawk_sink_client_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/sink:nighthawk_sink_client",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:statusor_lib_with_external_headers",
        "@envoy//source/common/protobuf:protobuf_with_external_headers",
    ],
)

envoy_cc_library(
    name = "sink_impl_lib",
    srcs = [
        "sink_impl.cc",
    ],
    hdrs = [
        "sink_impl.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//include/nighthawk/sink:sink_lib",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/common:random_generator_lib_with_external_headers",
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
        "//api/client:grpc_service_lib",
        "//api/sink:sink_grpc_lib",
        "//source/client:nighthawk_client_lib",
        "//source/sink:nighthawk_sink_client_impl",
        "//source/sink:sink_impl_lib",
        "@envoy//source/common/common:thread_lib_with_external_headers",
        "@envoy_api//envoy/config/core/v3:pkg_cc_proto",
    ],
)
